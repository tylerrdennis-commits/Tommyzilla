import { chromium } from 'playwright';
import { listShownCharacters } from '../src/character/catalog.js';

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
const errors = [];
page.on('pageerror', (err) => errors.push(String(err)));
page.on('console', (msg) => {
  if (msg.type() === 'error') errors.push(msg.text());
});

await page.goto('http://127.0.0.1:5173/', { waitUntil: 'networkidle' });
await page.waitForFunction(() => window.__tommyzilla);

const identities = await page.evaluate(() => window.__tommyzilla.identities());
const buttons = await page.locator('.roster-item').all();
const buttonIds = await Promise.all(buttons.map(async (button) => ({
  id: await button.getAttribute('data-character-id'),
  label: (await button.innerText()).trim(),
})));

const failed = [];
if (errors.length) failed.push(`page errors: ${errors.join('; ')}`);

const catalog = listShownCharacters();
if (identities.length !== catalog.length) failed.push(`expected ${catalog.length} heroes, saw ${identities.length}`);

for (const character of catalog) {
  const shown = identities.find((entry) => entry.id === character.id);
  const button = buttonIds.find((entry) => entry.id === character.id);
  if (!shown) {
    failed.push(`missing spawned hero ${character.id}`);
    continue;
  }
  if (shown.displayName !== character.displayName) {
    failed.push(`${character.id} display ${shown.displayName}`);
  }
  if (shown.nameplate !== character.displayName) {
    failed.push(`${character.id} nameplate ${shown.nameplate}`);
  }
  if (shown.modelKind !== character.modelKind) {
    failed.push(`${character.id} kind ${shown.modelKind}`);
  }
  if (shown.body !== character.palette.body.slice(1) || shown.limb !== character.palette.limb.slice(1)) {
    failed.push(`${character.id} skin body=${shown.body} limb=${shown.limb}`);
  }
  if (!button || button.label !== character.displayName) {
    failed.push(`${character.id} roster label ${button?.label}`);
  }
}

const yeti = catalog.find((character) => character.id === 'yeti');
await page.locator('.roster-item[data-character-id="yeti"]').click();
await page.click('#play');
await page.waitForTimeout(200);
const played = await page.evaluate(() => window.__tommyzilla.samplePose());
const hint = (await page.locator('#play-name').innerText()).trim();
const visible = await page.evaluate(() => window.__tommyzilla.identities().filter((entry) => entry.visible).map((entry) => entry.id));
if (played.displayName !== yeti.displayName) failed.push(`play pose named ${played.displayName}`);
if (hint !== yeti.displayName) failed.push(`play hint ${hint}`);
if (visible.length !== 1 || visible[0] !== 'yeti') failed.push(`visible while playing: ${visible.join(',')}`);

await page.screenshot({ path: '/tmp/tommyzilla-verify/names-play-yeti.png' });
await page.keyboard.press('Escape');
await page.waitForTimeout(250);
await page.screenshot({ path: '/tmp/tommyzilla-verify/names-lineup.png' });

const report = { failed, identities, buttonIds, played, hint, visible };
console.log(JSON.stringify(report, null, 2));
await browser.close();
if (failed.length) process.exit(1);
