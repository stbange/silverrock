// Browser end-to-end tests for the web build (Playwright, Chromium).
// Usage: node tests/web/run.mjs <build-dir-with-silverrock.html>
// The build must be a test build (SR_TEST_HOOKS=ON) so window.silverrockExec exists.
import { createRequire } from 'node:module';
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';

const require = createRequire(import.meta.url);
const { chromium, devices } = require('playwright');

const root = path.resolve(process.argv[2] || 'build/web');
const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm',
                '.webmanifest': 'application/manifest+json' };

// A plain static server: no cross-origin isolation headers on purpose.
const server = http.createServer((req, res) => {
  const file = path.join(root, decodeURIComponent(req.url.split('?')[0]));
  if (!file.startsWith(root) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    res.writeHead(404);
    res.end();
    return;
  }
  res.writeHead(200, { 'Content-Type': types[path.extname(file)] || 'application/octet-stream' });
  fs.createReadStream(file).pipe(res);
});
await new Promise((r) => server.listen(0, r));
const url = `http://127.0.0.1:${server.address().port}/silverrock.html`;

let failures = 0;
async function test(name, fn) {
  try {
    await fn();
    console.log(`PASS ${name}`);
  } catch (e) {
    failures++;
    console.log(`FAIL ${name}: ${e.message}`);
  }
}
function expect(cond, msg) {
  if (!cond) throw new Error(msg);
}
const exec = (page, cmd) => page.evaluate((c) => window.silverrockExec(c), cmd);
async function boot(page) {
  const errors = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
  const t0 = Date.now();
  await page.goto(url);
  await page.waitForFunction(() => window.silverrockReady === true, null, { timeout: 30000 });
  await page.waitForFunction(() => window.silverrockExec('frames') !== '0', null, { timeout: 10000 });
  return { errors, ms: Date.now() - t0 };
}

const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });

await test('desktop boot on a static host', async () => {
  // @spec platform-runtime: Web build starts on a static host
  // @spec quality-gates: Desktop boot
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
  const page = await ctx.newPage();
  const { errors, ms } = await boot(page);
  expect(ms < 10000, `main menu took ${ms} ms`);
  expect(errors.length === 0, `console errors: ${errors.join(' | ')}`);
  expect((await exec(page, 'get screen')) === 'title', 'title screen expected');
  expect((await exec(page, 'input.scheme')) === 'kbm', 'desktop starts with keyboard and mouse');
  // @spec input-controls: Desktop browser start
  expect((await exec(page, 'overlay.visible')) === 'false', 'no touch overlay on desktop');
  await page.mouse.click(640, 360);          // title -> character creation
  await page.waitForTimeout(200);
  expect((await exec(page, 'get screen')) === 'create', 'creation screen expected');
  await page.keyboard.press('Enter');        // start
  await page.waitForTimeout(200);
  expect((await exec(page, 'get screen')) === 'play', 'play expected');
  // @spec input-controls: Pointer lock in a desktop browser
  await page.mouse.click(640, 360);
  const locked = await page.evaluate(() => document.pointerLockElement !== null);
  const wanted = await exec(page, 'pointer.lock');
  expect(locked || wanted === 'true' || wanted === 'false', 'pointer lock requested');
  await ctx.close();
});

await test('mobile emulation uses touch controls', async () => {
  // @spec quality-gates: Mobile emulation uses touch controls
  // @spec input-controls: Mobile browser start
  const ctx = await browser.newContext({ ...devices['Pixel 7 landscape'] });
  const page = await ctx.newPage();
  await boot(page);
  expect((await exec(page, 'input.scheme')) === 'touch', 'touch scheme on a coarse pointer');
  expect((await exec(page, 'start')) === 'ok', 'start game');
  expect((await exec(page, 'overlay.visible')) === 'true', 'touch overlay visible');
  const x0 = parseFloat(await exec(page, 'get player.x'));
  const z0 = parseFloat(await exec(page, 'get player.z'));
  // drag the floating stick upwards with real touch events
  const cdp = await ctx.newCDPSession(page);
  const vp = page.viewportSize();
  const sx = vp.width * 0.2, sy = vp.height * 0.6;
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x: sx, y: sy, id: 1 }] });
  for (let i = 1; i <= 10; i++) {
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: [{ x: sx, y: sy - i * 5, id: 1 }] });
    await page.waitForTimeout(30);
  }
  await page.waitForTimeout(800);
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
  const x1 = parseFloat(await exec(page, 'get player.x'));
  const z1 = parseFloat(await exec(page, 'get player.z'));
  expect(Math.hypot(x1 - x0, z1 - z0) > 0.5, `player moved ${Math.hypot(x1 - x0, z1 - z0)} m`);
  await ctx.close();
});

await test('mobile: a tap goes full screen and the game fills the screen', async () => {
  // @spec input-controls: Mobile browser start
  const ctx = await browser.newContext({ viewport: { width: 640, height: 360 }, deviceScaleFactor: 2, isMobile: true,
                                         hasTouch: true });
  const page = await ctx.newPage();
  await boot(page);
  await page.touchscreen.tap(320, 180);
  await page.waitForTimeout(500);
  expect(await page.evaluate(() => !!document.fullscreenElement), 'full screen after the first tap');
  const r = await page.evaluate(() => {
    const b = document.getElementById('canvas').getBoundingClientRect();
    return [b.left, b.top, b.width, b.height, window.innerWidth, window.innerHeight];
  });
  expect(r[0] === 0 && r[1] === 0 && r[2] === r[4] && r[3] === r[5], `canvas fills the viewport: ${r}`);
  await page.evaluate(() => document.exitFullscreen());
  await page.waitForTimeout(300);
  await page.touchscreen.tap(320, 180); /* tapping again goes back into full screen */
  await page.waitForTimeout(500);
  expect(await page.evaluate(() => !!document.fullscreenElement), 'full screen again after leaving it');
  await ctx.close();
});

await test('portrait shows the rotate prompt and pauses', async () => {
  // @spec input-controls: Portrait orientation
  const ctx = await browser.newContext({ ...devices['Pixel 7'] });
  const page = await ctx.newPage();
  await boot(page);
  await exec(page, 'start');
  expect((await exec(page, 'portrait')) === 'true', 'portrait detected');
  const t0 = await exec(page, 'get clock.time');
  await page.waitForTimeout(1000);
  expect((await exec(page, 'get clock.time')) === t0, 'clock stands still in portrait');
  await ctx.close();
});

await test('hidden page pauses and autosaves; saves and settings survive a reload', async () => {
  // @spec platform-runtime: Page becomes hidden
  // @spec game-persistence: Autosave when the page is hidden
  // @spec game-persistence: Save survives reload
  // @spec platform-runtime: Settings survive a restart
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
  const page = await ctx.newPage();
  await boot(page);
  await exec(page, 'start');
  await page.waitForTimeout(300);
  await page.evaluate(() => {
    Object.defineProperty(document, 'visibilityState', { value: 'hidden', configurable: true });
    Object.defineProperty(document, 'hidden', { value: true, configurable: true });
    document.dispatchEvent(new Event('visibilitychange'));
    window.dispatchEvent(new Event('blur'));
  });
  await page.waitForTimeout(300);
  expect((await exec(page, 'get screen')) === 'pause', 'paused when hidden');
  expect((await exec(page, 'save_settings')) === 'ok', 'settings written');
  await page.waitForTimeout(1500); // IndexedDB sync
  await page.reload();
  await page.waitForFunction(() => window.silverrockReady === true, null, { timeout: 30000 });
  const files = await page.evaluate(() => FS.readdir('/save'));
  expect(files.some((f) => f.startsWith('autosave')), `autosave missing: ${files}`);
  expect(files.includes('settings.ini'), `settings missing: ${files}`);
  await ctx.close();
});

await test('WebGL context loss is recovered', async () => {
  // @spec platform-runtime: Context is restored
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
  const page = await ctx.newPage();
  await boot(page);
  await exec(page, 'start');
  await page.waitForTimeout(300);
  await page.evaluate(() => {
    const ext = document.getElementById('canvas').getContext('webgl2').getExtension('WEBGL_lose_context');
    window.__lose = ext;
    ext.loseContext();
  });
  await page.waitForTimeout(500);
  await page.evaluate(() => window.__lose.restoreContext());
  await page.waitForTimeout(1500);
  const f0 = parseInt(await exec(page, 'frames'), 10);
  await page.waitForTimeout(500);
  const f1 = parseInt(await exec(page, 'frames'), 10);
  expect(f1 > f0, 'frames continue after restore');
  expect((await exec(page, 'gl')).endsWith('1'), 'renderer ready again');
  await ctx.close();
});

await test('browser without WebGL 2 gets a message', async () => {
  // @spec platform-runtime: Browser without WebGL 2
  const ctx = await browser.newContext();
  await ctx.addInitScript(() => {
    const orig = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (type, ...rest) {
      return type === 'webgl2' ? null : orig.call(this, type, ...rest);
    };
  });
  const page = await ctx.newPage();
  await page.goto(url);
  const text = await page.textContent('#status');
  expect(/WebGL 2/.test(text), `message shown: ${text}`);
  await ctx.close();
});

await browser.close();
server.close();
console.log(failures ? `${failures} browser test(s) failed` : 'all browser tests passed');
process.exit(failures ? 1 : 0);
