import { chromium } from 'playwright';
import fs from 'node:fs';

const outDir = '/tmp/tommyzilla-verify';
fs.mkdirSync(outDir, { recursive: true });

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
const errors = [];
page.on('pageerror', (err) => errors.push(String(err)));
page.on('console', (msg) => {
  if (msg.type() === 'error') errors.push(msg.text());
});

await page.goto('http://127.0.0.1:5173/', { waitUntil: 'networkidle' });
await page.waitForFunction(() => window.__tommyzilla);

const menuSamples = [];
for (let i = 0; i < 8; i += 1) {
  menuSamples.push(await page.evaluate(() => window.__tommyzilla.samplePose()));
  if (i === 1) await page.screenshot({ path: `${outDir}/menu-stride-a.png` });
  if (i === 5) await page.screenshot({ path: `${outDir}/menu-stride-b.png` });
  await page.waitForTimeout(120);
}

await page.click('#play');
await page.waitForTimeout(200);
await page.keyboard.down('KeyW');
const playSamples = [];
for (let i = 0; i < 6; i += 1) {
  playSamples.push(await page.evaluate(() => window.__tommyzilla.samplePose()));
  if (i === 2) await page.screenshot({ path: `${outDir}/play-walk.png` });
  await page.waitForTimeout(100);
}
await page.keyboard.up('KeyW');
await page.waitForTimeout(250);
const idle = await page.evaluate(() => window.__tommyzilla.samplePose());
await page.screenshot({ path: `${outDir}/play-idle.png` });
await page.keyboard.press('Escape');
await page.waitForTimeout(300);
const back = await page.evaluate(() => window.__tommyzilla.samplePose());
await page.screenshot({ path: `${outDir}/menu-return.png` });

function range(samples, key) {
  const values = samples.map((sample) => sample[key]);
  return Math.max(...values) - Math.min(...values);
}

const report = {
  errors,
  menuClip: menuSamples.map((sample) => sample.clip),
  menuArmHorizontal: menuSamples.some((sample) => sample.leftArmHorizontal || sample.rightArmHorizontal),
  menuThighRange: range(menuSamples, 'leftThighX'),
  menuArmY: menuSamples.map((sample) => Number(sample.leftArmY.toFixed(3))),
  playClip: playSamples.map((sample) => sample.clip),
  playMode: playSamples.map((sample) => sample.mode),
  playThighRange: range(playSamples, 'leftThighX'),
  playArmHorizontal: playSamples.some((sample) => sample.leftArmHorizontal || sample.rightArmHorizontal),
  idle,
  back,
};

const failed = [];
if (errors.length) failed.push('page errors');
if (menuSamples.some((sample) => sample.clip !== 'Walk' || sample.mode !== 'menu')) failed.push('menu not walking');
if (report.menuArmHorizontal) failed.push('menu arms horizontal');
if (report.menuThighRange < 0.4) failed.push(`menu thigh range ${report.menuThighRange}`);
if (menuSamples.some((sample) => sample.leftArmY > -0.45 || sample.rightArmY > -0.45)) failed.push('menu arms not down');
if (playSamples.some((sample) => sample.mode !== 'play' || sample.clip !== 'Walk')) failed.push('play not walking');
if (report.playArmHorizontal) failed.push('play arms horizontal');
if (report.playThighRange < 0.2) failed.push(`play thigh range ${report.playThighRange}`);
if (idle.clip !== 'Idle' || idle.leftArmHorizontal || idle.rightArmHorizontal) failed.push('idle still T-pose or wrong clip');
if (idle.leftArmY > -0.7 || idle.rightArmY > -0.7) failed.push('idle arms not down');
if (back.mode !== 'menu' || back.clip !== 'Walk') failed.push('esc did not resume menu walk');

fs.writeFileSync(`${outDir}/report.json`, JSON.stringify({ failed, report }, null, 2));
console.log(JSON.stringify({ failed, report }, null, 2));
await browser.close();
if (failed.length) process.exit(1);
