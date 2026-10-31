import { chromium } from 'playwright';
import fs from 'node:fs';
import { listVariants } from '../src/character/variants.js';
import { SHINY_EGG_COST, SHINY_ODDS_COPY, STARTING_EGGS } from '../src/character/shinyRoll.js';

const outDir = '/tmp/tommyzilla-verify';
fs.mkdirSync(outDir, { recursive: true });

const browser = await chromium.launch({
  headless: true,
  executablePath: '/usr/bin/google-chrome',
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
const errors = [];
page.on('pageerror', (err) => errors.push(String(err)));
page.on('console', (msg) => {
  if (msg.type() !== 'error') return;
  const url = msg.location()?.url ?? '';
  if (url.includes('favicon')) return;
  errors.push(`${msg.text()} ${url}`.trim());
});

await page.goto('http://127.0.0.1:5173/', { waitUntil: 'networkidle' });
await page.waitForFunction(() => window.__tommyzilla);

const failed = [];
if (errors.length) failed.push(`page errors: ${errors.join('; ')}`);

const catalog = listVariants();
const ui = await page.evaluate(() => window.__tommyzilla.variants());
const buttons = await page.locator('.variant-item').all();
const labels = [];
for (const button of buttons) {
  labels.push((await button.innerText()).replace(/^★\s*/, '').trim());
}

if (ui.length < 16) failed.push(`variant api ${ui.length}`);
if (buttons.length < 16) failed.push(`variant buttons ${buttons.length}`);
if (new Set(labels).size !== labels.length) failed.push(`duplicate labels ${labels.join('|')}`);
for (const variant of catalog) {
  if (!labels.includes(variant.displayName)) failed.push(`missing button ${variant.displayName}`);
}

const odds = (await page.locator('#shiny-odds').innerText()).trim();
const eggsText = (await page.locator('#egg-count').innerText()).trim();
if (odds !== SHINY_ODDS_COPY) failed.push(`odds copy ${odds}`);
if (!/not a guarantee/i.test(odds)) failed.push('odds copy omits guarantee warning');
if (!eggsText.includes(String(STARTING_EGGS))) failed.push(`starting eggs ${eggsText}`);

const blockedAtStart = await page.locator('#shiny-chance').isDisabled();
const startBlock = (await page.locator('#egg-block').innerText()).trim();
if (!blockedAtStart) failed.push('spend should be blocked before a variant is chosen');
if (!/choose a variant first/i.test(startBlock)) failed.push(`start block ${startBlock}`);

const refusedApi = await page.evaluate(() => window.__tommyzilla.spendShiny({ random: () => 0 }));
if (refusedApi.ok || refusedApi.spent !== 0 || refusedApi.eggs !== STARTING_EGGS) {
  failed.push(`spend without variant mutated eggs ${JSON.stringify(refusedApi)}`);
}

await page.locator('.variant-item[data-variant-id="moss-titan"]').click();
await page.waitForTimeout(200);
const selected = await page.evaluate(() => window.__tommyzilla.preview());
if (!selected || selected.id !== 'moss-titan' || selected.shiny || selected.spark) {
  failed.push(`preview ${JSON.stringify(selected)}`);
}
if (selected.nameplate !== 'Moss Titan') failed.push(`preview nameplate ${selected?.nameplate}`);
await page.screenshot({ path: `${outDir}/variant-preview.png` });

const low = await page.evaluate(() => window.__tommyzilla.setEggs(9));
if (low.eggs !== 9 || low.canSpend) failed.push(`low eggs state ${JSON.stringify(low)}`);
const lowDisabled = await page.locator('#shiny-chance').isDisabled();
const lowBlock = (await page.locator('#egg-block').innerText()).trim();
if (!lowDisabled) failed.push('spend button enabled with 9 eggs');
if (!/Need 10 eggs/.test(lowBlock) || !/you have 9/.test(lowBlock)) failed.push(`low block ${lowBlock}`);
const refusedLow = await page.evaluate(() => window.__tommyzilla.spendShiny({ random: () => 0 }));
if (refusedLow.ok || refusedLow.eggs !== 9 || refusedLow.spent !== 0) {
  failed.push(`low spend ${JSON.stringify(refusedLow)}`);
}
await page.screenshot({ path: `${outDir}/variant-need-eggs.png` });

await page.evaluate((cost) => window.__tommyzilla.setEggs(cost), SHINY_EGG_COST);
const enabled = await page.locator('#shiny-chance').isEnabled();
if (!enabled) failed.push('spend button stayed disabled with exactly 10 eggs');

const miss = await page.evaluate(() => window.__tommyzilla.spendShiny({ random: () => 0.95 }));
if (!miss.ok || miss.shiny || miss.eggs !== 0) failed.push(`miss roll ${JSON.stringify(miss)}`);
const missText = (await page.locator('#shiny-result').innerText()).trim();
if (!/no shiny/i.test(missText) || !/spent/i.test(missText)) failed.push(`miss text ${missText}`);
const afterMiss = await page.locator('#shiny-chance').isDisabled();
const afterMissBlock = (await page.locator('#egg-block').innerText()).trim();
if (!afterMiss || !/you have 0/.test(afterMissBlock)) failed.push(`post-miss block ${afterMissBlock}`);

await page.evaluate((cost) => window.__tommyzilla.setEggs(cost), SHINY_EGG_COST);
await page.locator('#shiny-chance').click();
const hit = await page.evaluate(() => window.__tommyzilla.eggState());
if (hit.eggs !== 0) failed.push(`button spend eggs ${hit.eggs}`);

await page.evaluate((cost) => window.__tommyzilla.setEggs(cost), SHINY_EGG_COST);
const shiny = await page.evaluate(() => window.__tommyzilla.spendShiny({ random: () => 0 }));
const moss = catalog.find((variant) => variant.id === 'moss-titan');
if (!shiny.ok || !shiny.shiny || !shiny.preview?.spark) {
  failed.push(`shiny roll ${JSON.stringify(shiny)}`);
}
if (shiny.preview?.body === moss.palette.body.slice(1)) {
  failed.push(`shiny kept regular body ${shiny.preview?.body}`);
}
if (shiny.preview?.body !== moss.shinyPalette.body.slice(1)) {
  failed.push(`shiny body ${shiny.preview?.body} expected ${moss.shinyPalette.body}`);
}
const shinyText = (await page.locator('#shiny-result').innerText()).trim();
if (!/Shiny Moss Titan/.test(shinyText)) failed.push(`shiny text ${shinyText}`);
const shinyLabel = (await page.locator('.variant-item[data-variant-id="moss-titan"]').innerText()).trim();
if (!shinyLabel.startsWith('★')) failed.push(`shiny chip ${shinyLabel}`);
await page.screenshot({ path: `${outDir}/variant-shiny.png` });

await page.click('#play');
await page.waitForTimeout(250);
const played = await page.evaluate(() => window.__tommyzilla.samplePose());
const hint = (await page.locator('#play-name').innerText()).trim();
if (played.displayName !== 'Moss Titan' || played.clip !== 'Walk' || played.mode !== 'play') {
  failed.push(`shiny play ${JSON.stringify(played)}`);
}
if (played.leftArmHorizontal || played.rightArmHorizontal) failed.push('shiny play arms horizontal');
if (hint !== 'Shiny Moss Titan') failed.push(`shiny hint ${hint}`);
const lineup = await page.evaluate(() => window.__tommyzilla.identities());
if (lineup.length !== 3) failed.push(`lineup grew to ${lineup.length}`);
if (lineup.some((entry) => entry.visible)) failed.push('base heroes stayed visible during variant play');
await page.screenshot({ path: `${outDir}/variant-shiny-play.png` });

await page.keyboard.up('KeyW');
await page.keyboard.press('Escape');
await page.waitForTimeout(250);
const back = await page.evaluate(() => window.__tommyzilla.samplePose());
if (back.mode !== 'menu' || back.clip !== 'Walk') failed.push(`esc ${JSON.stringify(back)}`);

const kept = await page.evaluate(() => {
  window.__tommyzilla.setEggs(20);
  const roll = window.__tommyzilla.spendShiny({ random: () => 0.99 });
  return { roll, preview: window.__tommyzilla.preview() };
});
if (!kept.preview?.shiny) failed.push(`miss cleared shiny ${JSON.stringify(kept)}`);
if (kept.roll.eggs !== 10) failed.push(`keep-shiny eggs ${kept.roll.eggs}`);

console.log(JSON.stringify({ failed, odds, eggsText, labels, shinyText, hint }, null, 2));
await browser.close();
if (failed.length) process.exit(1);
