import { chromium } from 'playwright';
import fs from 'node:fs';
import { listStones } from '../src/stones/catalog.js';

const outDir = '/tmp/tommyzilla-verify';
fs.mkdirSync(outDir, { recursive: true });

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
const errors = [];
page.on('pageerror', (err) => errors.push(String(err)));
page.on('console', (msg) => {
  if (msg.type() === 'error') errors.push(msg.text());
});

const failed = [];

function expect(condition, message) {
  if (!condition) failed.push(message);
}

await page.goto('http://127.0.0.1:5173/', { waitUntil: 'networkidle' });
await page.waitForFunction(() => window.__tommyzilla?.stones);

const catalog = listStones();
const rows = await page.locator('.stone-choice').all();
expect(rows.length === catalog.length, `expected ${catalog.length} stone choices, saw ${rows.length}`);

for (const stone of catalog) {
  const button = page.locator(`.stone-choice[data-stone-id="${stone.id}"]`);
  const text = (await button.innerText()).replace(/\s+/g, ' ').trim();
  const pressed = await button.getAttribute('aria-pressed');
  expect(text.includes(stone.name), `${stone.id} name missing from "${text}"`);
  expect(text.includes(stone.explanation), `${stone.id} explanation missing from "${text}"`);
  expect(pressed === 'false', `${stone.id} started pressed`);
}

await page.screenshot({ path: `${outDir}/stones-menu.png` });

await page.locator('.stone-choice[data-stone-id="space"]').click();
await page.locator('.stone-choice[data-stone-id="mind"]').click();
const pressedIds = await page.evaluate(() => window.__tommyzilla.stones.selected());
expect(pressedIds.includes('space') && pressedIds.includes('mind'), `selected ${pressedIds.join(',')}`);
expect(
  (await page.locator('.stone-choice[data-stone-id="space"]').getAttribute('aria-pressed')) === 'true',
  'space button not equipped',
);
await page.locator('.stone-choice[data-stone-id="space"]').click();
await page.locator('.stone-choice[data-stone-id="mind"]').click();
expect((await page.evaluate(() => window.__tommyzilla.stones.selected())).length === 0, 'stones did not toggle off');

async function playFor(ms) {
  await page.click('#play');
  await page.waitForTimeout(ms);
  const snap = await page.evaluate(() => window.__tommyzilla.stones.snapshot());
  const pose = await page.evaluate(() => window.__tommyzilla.samplePose());
  return { snap, pose };
}

async function backToMenu() {
  await page.keyboard.press('Escape');
  await page.waitForTimeout(200);
}

const baseline = await playFor(900);
expect(baseline.pose.clip === 'Walk' && baseline.pose.mode === 'play', 'baseline play is not walking');
expect(baseline.pose.leftArmHorizontal === false && baseline.pose.rightArmHorizontal === false, 'baseline arms horizontal');
expect(baseline.snap.heroScale === 1, `baseline scale ${baseline.snap.heroScale}`);
expect(baseline.snap.background === '141a1e', `baseline field ${baseline.snap.background}`);
expect(baseline.snap.auraVisible === false, 'baseline aura visible');
expect(baseline.snap.senseText === '', `baseline sense "${baseline.snap.senseText}"`);
expect(baseline.snap.readoutText === 'No Infinity Stones equipped.', `baseline readout "${baseline.snap.readoutText}"`);
expect(baseline.snap.heroZ > 1, `baseline did not move (${baseline.snap.heroZ})`);
await page.screenshot({ path: `${outDir}/stones-play-none.png` });
await backToMenu();

await page.evaluate(() => window.__tommyzilla.stones.toggle('space'));
const spaceRun = await playFor(900);
expect(spaceRun.pose.clip === 'Walk', `space clip ${spaceRun.pose.clip}`);
expect(spaceRun.snap.heroZ > baseline.snap.heroZ * 1.25, `space stride ${spaceRun.snap.heroZ} vs ${baseline.snap.heroZ}`);
expect(spaceRun.snap.readoutText.includes(catalog.find((stone) => stone.id === 'space').explanation), 'space explanation missing in play');
await page.screenshot({ path: `${outDir}/stones-play-space.png` });
await backToMenu();
await page.evaluate(() => window.__tommyzilla.stones.toggle('space'));

await page.evaluate(() => window.__tommyzilla.stones.toggle('time'));
const timeRun = await playFor(900);
expect(timeRun.pose.clip === 'Walk', `time clip ${timeRun.pose.clip}`);
expect(timeRun.pose.leftArmHorizontal === false, 'time stone froze a T-pose');
expect(timeRun.snap.heroZ < baseline.snap.heroZ * 0.7, `time stride ${timeRun.snap.heroZ} vs ${baseline.snap.heroZ}`);
expect(timeRun.snap.readoutText.includes(catalog.find((stone) => stone.id === 'time').explanation), 'time explanation missing in play');
await backToMenu();
await page.evaluate(() => window.__tommyzilla.stones.toggle('time'));

await page.evaluate(() => window.__tommyzilla.stones.toggle('power'));
const powerRun = await playFor(400);
expect(Math.abs(powerRun.snap.heroScale - 1.45) < 0.01, `power scale ${powerRun.snap.heroScale}`);
expect(powerRun.pose.clip === 'Walk', `power clip ${powerRun.pose.clip}`);
expect(powerRun.pose.leftArmHorizontal === false && powerRun.pose.rightArmHorizontal === false, 'power stone T-pose');
await page.screenshot({ path: `${outDir}/stones-play-power.png` });
await backToMenu();
const powerMenu = await page.evaluate(() => window.__tommyzilla.stones.snapshot());
expect(powerMenu.heroScale === 1, `power scale leaked to the menu (${powerMenu.heroScale})`);
await page.evaluate(() => window.__tommyzilla.stones.toggle('power'));

await page.evaluate(() => window.__tommyzilla.stones.toggle('reality'));
const realityRun = await playFor(300);
expect(realityRun.snap.background === '3a1820', `reality field ${realityRun.snap.background}`);
expect(realityRun.snap.fog === '5c2832', `reality fog ${realityRun.snap.fog}`);
expect(realityRun.snap.ground === '6a3038', `reality ground ${realityRun.snap.ground}`);
expect(realityRun.snap.readoutText.includes(catalog.find((stone) => stone.id === 'reality').explanation), 'reality explanation missing in play');
await page.screenshot({ path: `${outDir}/stones-play-reality.png` });
await backToMenu();
const restored = await page.evaluate(() => window.__tommyzilla.stones.snapshot());
expect(restored.background === '141a1e', `field tint leaked to the menu (${restored.background})`);
await page.evaluate(() => window.__tommyzilla.stones.toggle('reality'));

await page.evaluate(() => window.__tommyzilla.stones.toggle('mind'));
const mindRun = await playFor(500);
expect(/^Facing north · pace \d+\.\d$/.test(mindRun.snap.senseText), `mind sense "${mindRun.snap.senseText}"`);
expect(mindRun.snap.readoutText.includes(catalog.find((stone) => stone.id === 'mind').explanation), 'mind explanation missing in play');
await page.screenshot({ path: `${outDir}/stones-play-mind.png` });
await backToMenu();
await page.evaluate(() => window.__tommyzilla.stones.toggle('mind'));

await page.evaluate(() => window.__tommyzilla.stones.toggle('soul'));
const soulRun = await playFor(400);
expect(soulRun.snap.auraVisible === true, 'soul aura missing');
expect(soulRun.snap.readoutText.includes(catalog.find((stone) => stone.id === 'soul').explanation), 'soul explanation missing in play');
await page.screenshot({ path: `${outDir}/stones-play-soul.png` });
await backToMenu();
const soulMenu = await page.evaluate(() => window.__tommyzilla.stones.snapshot());
expect(soulMenu.auraVisible === false, 'soul aura leaked to the menu');
await page.evaluate(() => window.__tommyzilla.stones.toggle('soul'));

for (const stone of catalog) {
  await page.evaluate((id) => window.__tommyzilla.stones.toggle(id), stone.id);
}
const full = await playFor(300);
for (const stone of catalog) {
  expect(full.snap.readoutText.includes(stone.explanation), `full readout missing ${stone.id}`);
}
expect(full.pose.clip === 'Walk', `full loadout clip ${full.pose.clip}`);
await page.screenshot({ path: `${outDir}/stones-play-all.png` });
await backToMenu();

const menuPose = await page.evaluate(() => window.__tommyzilla.samplePose());
expect(menuPose.mode === 'menu' && menuPose.clip === 'Walk', `menu after stones ${menuPose.mode} ${menuPose.clip}`);
expect(menuPose.leftArmHorizontal === false && menuPose.rightArmHorizontal === false, 'menu T-pose after stones');

await page.setViewportSize({ width: 390, height: 844 });
await page.waitForTimeout(200);
const mobileBox = await page.locator('#infinity-stones').boundingBox();
expect(Boolean(mobileBox && mobileBox.height > 40 && mobileBox.width > 200), 'stones panel missing on a narrow screen');
const mobileText = (await page.locator('#infinity-stones').evaluate((el) => el.textContent)).replace(/\s+/g, ' ');
for (const stone of catalog) {
  expect(mobileText.includes(stone.explanation), `narrow screen missing ${stone.id} explanation`);
}
await page.screenshot({ path: `${outDir}/stones-mobile.png` });

if (errors.length) failed.push(`page errors: ${errors.join('; ')}`);

const report = { failed, errors };
fs.writeFileSync(`${outDir}/stones-report.json`, JSON.stringify(report, null, 2));
console.log(JSON.stringify(report, null, 2));
await browser.close();
if (failed.length) process.exit(1);
