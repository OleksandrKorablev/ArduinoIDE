let tempChart, humidityChart, gasChart;

function buildCharts(data) {
  const timestamps = data.map(d => d.timestamp);
  const temperatures = data.map(d => d.temperature);
  const humidity = data.map(d => d.humidity);
  const CO = data.map(d => d.CO);
  const CH4 = data.map(d => d.CH4);

// Clear previous graphs
  if (tempChart) tempChart.destroy();
  if (humidityChart) humidityChart.destroy();
  if (gasChart) gasChart.destroy();

  tempChart = new Chart(document.getElementById("tempChart"), {
    type: 'line',
    data: {
      labels: timestamps,
      datasets: [{
        label: 'Температура (°C)',
        data: temperatures,
        borderColor: 'red',
        fill: false
      }]
    },
    options: {
      responsive: true,
      scales: {
        x: { title: { display: true, text: 'Час' } },
        y: { title: { display: true, text: 'Температура' } }
      }
    }
  });

  humidityChart = new Chart(document.getElementById("humidityChart"), {
    type: 'line',
    data: {
      labels: timestamps,
      datasets: [{
        label: 'Вологість (%)',
        data: humidity,
        borderColor: 'blue',
        fill: false
      }]
    },
    options: {
      responsive: true,
      scales: {
        x: { title: { display: true, text: 'Час' } },
        y: { title: { display: true, text: 'Вологість' } }
      }
    }
  });

  gasChart = new Chart(document.getElementById("gasChart"), {
    type: 'line',
    data: {
      labels: timestamps,
      datasets: [
        {
          label: 'CO',
          data: CO,
          borderColor: 'green',
          fill: false
        },
        {
          label: 'CH₄',
          data: CH4,
          borderColor: 'orange',
          fill: false
        }
      ]
    },
    options: {
      responsive: true,
      scales: {
        x: { title: { display: true, text: 'Час' } },
        y: { title: { display: true, text: 'Концентрація (ppm)' } }
      }
    }
  });
}
