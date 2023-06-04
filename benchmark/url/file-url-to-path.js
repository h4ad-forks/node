'use strict';
const common = require('../common.js');
const { fileURLToPath } = require('url');

const toUrl = '(url) ';
const validUrls = [
  'file:///home/user/test/index.js',
  'file:///home/user/test%20index.js',
  'file:///home/user/test%2Findex.js',
  'file://google.com/home/user/test%2Findex.js',
  'http://google.com/home/test.js',
];
const bench = common.createBenchmark(main, {
  url: [
    ...validUrls,
    ...validUrls.map(url => toUrl + url),
    'not-even-a-url',
  ],
  n: [1e5],
});

function main({ url, n }) {
  bench.start();

  const urlToTest = url.startsWith(toUrl) ? new URL(url.slice(toUrl.length)) : url;

  for (let i = 0; i < n; i += 1) {
    try {
      fileURLToPath(urlToTest);
    } catch { }
  }
  bench.end(n);
}
