import { chromium } from 'playwright';
import fs from 'node:fs';
import { bossBanner } from '../src/boss/fight.js';
import { listEncounters } from '../src/boss/encounters.js';
import { getVariant } from '../src/character/variants.js';

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

const failed = [];
function expect(condition, message) {
  if (!condition) failed.push(message);
}

await page.goto('http://127.0.0.1:5173/', { waitUntil: 'networkidle' });
await page.waitForFunction(() => window.__tommyzilla?.bosses && window.__tommyzilla?.stones);

await page.click('#play');
await page.waitForTimeout(250);

const opening = await page.evaluate(() => ({
  current: window.__tommyzilla.bosses.current(),
  banner: window.__tommyzilla.bosses.banner(),
  actor: window.__tommyzilla.bosses.actor(),
  pose: window.__tommyzilla.samplePose(),
}));

expect(opening.banner === bossBanner(opening.current), `banner "${opening.banner}"`);
expect((await page.locator('#boss-banner').innerText()).trim() === opening.banner, 'dom banner drifted');
expect(opening.actor, 'boss mesh missing');
expect(opening.actor.visible, 'boss hidden in play');
expect(opening.actor.id === opening.current.id, 'actor encounter mismatch');
expect(opening.actor.variantId === opening.current.variantId, 'actor variant mismatch');
expect(opening.actor.displayName === opening.current.displayName, 'actor name mismatch');
expect(opening.actor.nameplate === opening.current.displayName, `nameplate ${opening.actor.nameplate}`);
expect(opening.actor.shiny === opening.current.shiny, 'shiny flag mismatch');
const variant = getVariant(opening.current.variantId);
const expectedBody = (opening.current.shiny ? variant.shinyPalette : variant.palette).body.slice(1);
expect(opening.actor.body === expectedBody, `body ${opening.actor.body} vs ${expectedBody}`);
expect(opening.actor.spark === opening.current.shiny, 'shiny spark mismatch');
expect(opening.actor.clip === (opening.current.moving ? 'Walk' : 'Idle'), `boss clip ${opening.actor.clip}`);
expect(opening.actor.clip !== 'TPose', 'boss stuck in T-pose');
expect(opening.pose.mode === 'play' && opening.pose.clip === 'Walk', `player ${opening.pose.mode} ${opening.pose.clip}`);
expect(!opening.pose.leftArmHorizontal && !opening.pose.rightArmHorizontal, 'player arms horizontal during boss fight');
await page.screenshot({ path: `${outDir}/boss-opening.png` });

let guard = 0;
let moving = opening.current;
while (!moving.moving && guard < 16) {
  moving = await page.evaluate(() => window.__tommyzilla.bosses.advance());
  guard += 1;
}
expect(moving.moving, 'no moving boss pattern in a full cycle');
const before = await page.evaluate(() => window.__tommyzilla.bosses.actor());
await page.waitForTimeout(900);
const after = await page.evaluate(() => ({
  actor: window.__tommyzilla.bosses.actor(),
  current: window.__tommyzilla.bosses.current(),
}));
const traveled = Math.hypot(after.actor.x - before.x, after.actor.z - before.z);
expect(after.current.id === moving.id, 'boss changed during the motion sample');
expect(traveled > 0.15, `boss did not travel (${traveled})`);
expect(after.actor.clip === 'Walk', `moving boss clip ${after.actor.clip}`);
await page.screenshot({ path: `${outDir}/boss-moving.png` });

for (let i = 0; i < 16; i += 1) {
  if (i % 2 === 0) await page.click('#next-boss');
  else await page.keyboard.press('KeyN');
  await page.waitForTimeout(40);
  const snap = await page.evaluate(() => ({
    current: window.__tommyzilla.bosses.current(),
    banner: window.__tommyzilla.bosses.banner(),
    actor: window.__tommyzilla.bosses.actor(),
  }));
  expect(snap.banner === bossBanner(snap.current), `banner after advance ${i}: ${snap.banner}`);
  expect(snap.actor.variantId === snap.current.variantId, `mesh lagged on ${snap.current.id}`);
  expect(snap.actor.nameplate === snap.current.displayName, `nameplate lagged on ${snap.current.id}`);
  expect(snap.actor.clip !== 'TPose', `T-pose on ${snap.current.id}`);
}

const history = await page.evaluate(() => window.__tommyzilla.bosses.history());
const rosterIds = listEncounters().map((encounter) => encounter.id);
const roster = new Set(rosterIds);
expect(history.length > roster.size, `history shorter than a full cycle (${history.length})`);
for (let start = 0; start + roster.size <= history.length; start += roster.size) {
  const cycle = history.slice(start, start + roster.size);
  expect(new Set(cycle).size === roster.size, `cycle at ${start} repeated: ${cycle.join(',')}`);
  for (const id of cycle) expect(roster.has(id), `unknown encounter ${id}`);
}
for (let i = 1; i < history.length; i += 1) {
  expect(history[i] !== history[i - 1], `adjacent repeat at ${i}: ${history[i]}`);
}
if (history.length > roster.size) {
  expect(history[roster.size] !== history[roster.size - 1], 'new cycle repeated the closing boss');
}

const playerAfter = await page.evaluate(() => window.__tommyzilla.samplePose());
expect(playerAfter.mode === 'play' && playerAfter.clip === 'Walk', `player left the walk after boss swaps (${playerAfter.clip})`);
await page.screenshot({ path: `${outDir}/boss-next.png` });

await page.keyboard.press('Escape');
await page.waitForTimeout(200);
const menuBoss = await page.evaluate(() => window.__tommyzilla.bosses.actor());
const menuPose = await page.evaluate(() => window.__tommyzilla.samplePose());
expect(menuBoss && menuBoss.visible === false, 'boss stayed visible on the title');
expect(menuPose.mode === 'menu' && menuPose.clip === 'Walk', `menu after boss ${menuPose.mode} ${menuPose.clip}`);

await page.evaluate(() => window.__tommyzilla.stones.toggle('power'));
await page.click('#play');
await page.waitForTimeout(200);
const powered = await page.evaluate(() => ({
  stones: window.__tommyzilla.stones.snapshot(),
  actor: window.__tommyzilla.bosses.actor(),
  pose: window.__tommyzilla.samplePose(),
}));
expect(Math.abs(powered.stones.heroScale - 1.45) < 0.01, `power scale ${powered.stones.heroScale}`);
expect(powered.actor.visible, 'boss missing while a stone is equipped');
expect(powered.pose.clip === 'Walk', `power clip ${powered.pose.clip}`);
await page.screenshot({ path: `${outDir}/boss-with-power-stone.png` });
await page.keyboard.press('Escape');
await page.waitForTimeout(150);
const restored = await page.evaluate(() => window.__tommyzilla.stones.snapshot());
expect(restored.heroScale === 1, `power scale leaked (${restored.heroScale})`);

if (errors.length) failed.push(`page errors: ${errors.join('; ')}`);
const report = { failed, history, errors };
fs.writeFileSync(`${outDir}/boss-report.json`, JSON.stringify(report, null, 2));
console.log(JSON.stringify(report, null, 2));
await browser.close();
if (failed.length) process.exit(1);
