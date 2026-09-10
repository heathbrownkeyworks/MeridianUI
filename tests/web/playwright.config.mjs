import { defineConfig } from '@playwright/test';
export default defineConfig({
  testDir: '.', testMatch: '**/*.spec.mjs', fullyParallel: true,
  reporter: 'list', use: { headless: true, viewport: {width:1000,height:800} }
});
