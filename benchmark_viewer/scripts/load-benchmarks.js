// scripts/load-benchmarks.js

const OS_DIRS = ['Linux', 'macOS', 'Windows'];
const REPO = 'GRIS-UdeM/AlgoGRIS';
const BRANCH = 'benchmarks';

async function fetchBenchmarkFiles() {
  const allBenchmarks = {}; // { commitHash: { os: xmlDoc } }

  for (const os of OS_DIRS) {
    const apiURL = `https://api.github.com/repos/${REPO}/contents/${os}?ref=${BRANCH}`;
    const response = await fetch(apiURL);
    if (!response.ok) {
      console.error(`Failed to fetch ${os} directory from GitHub:`, response.statusText);
      continue;
    }
    const files = await response.json();

    for (const file of files) {
      if (!file.name.endsWith('.xml')) continue;
      const commitHash = file.name.replace('.xml', '');

      if (!allBenchmarks[commitHash]) {
        allBenchmarks[commitHash] = {};
      }

      const xmlRes = await fetch(file.download_url);
      const xmlText = await xmlRes.text();
      const parser = new DOMParser();
      const xmlDoc = parser.parseFromString(xmlText, 'application/xml');

      allBenchmarks[commitHash][os] = xmlDoc;
    }
  }

  return allBenchmarks;
}

function parseBenchmarks(allBenchmarks) {
  const resultsByTestName = {}; // { testName: { commitHash: { os: mean } } }

  for (const [commit, osData] of Object.entries(allBenchmarks)) {
    for (const [os, xmlDoc] of Object.entries(osData)) {
      const testCases = xmlDoc.querySelectorAll('TestCase');
      for (const testCase of testCases) {
        const testName = testCase.getAttribute('name');
        const benchNode = testCase.querySelector('BenchmarkResults mean');
        if (!benchNode) continue;

        const mean = parseFloat(benchNode.getAttribute('value'));

        if (!resultsByTestName[testName]) resultsByTestName[testName] = {};
        if (!resultsByTestName[testName][commit]) resultsByTestName[testName][commit] = {};

        resultsByTestName[testName][commit][os] = mean;
      }
    }
  }

  return resultsByTestName;
}

function renderCharts(resultsByTestName) {
  for (const [testName, commitData] of Object.entries(resultsByTestName)) {
    const container = document.createElement('div');
    container.innerHTML = `<h2>${testName}</h2><canvas id="chart-${testName}"></canvas>`;
    document.body.appendChild(container);

    const labels = Object.keys(commitData);
    const datasets = OS_DIRS.map(os => {
      return {
        label: os,
        data: labels.map(commit => commitData[commit][os] || null),
        fill: false,
        borderColor: randomColor(),
        tension: 0.1,
      };
    });

    new Chart(document.getElementById(`chart-${testName}`), {
      type: 'line',
      data: { labels, datasets },
      options: {
        responsive: true,
        plugins: { legend: { position: 'top' }, title: { display: true, text: `${testName} Mean Time` } },
        scales: {
          y: {
            title: {
              display: true,
              text: 'Mean Time (ns)',
            },
            type: 'logarithmic',
          },
        },
      },
    });
  }
}

function randomColor() {
  return `hsl(${Math.floor(Math.random() * 360)}, 70%, 50%)`;
}

(async () => {
  const benchmarks = await fetchBenchmarkFiles();
  const parsed = parseBenchmarks(benchmarks);
  renderCharts(parsed);
})();
