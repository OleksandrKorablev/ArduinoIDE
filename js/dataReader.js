const basePath = "/DataFromMicrocontrollers/";
let allSensorData = [];
let currentFilter = "all";

async function loadSensorData() {
  const indexRes = await fetch(basePath + "index.json");
  const indexData = await indexRes.json();

  for (const file of indexData.files) {
    const res = await fetch(basePath + file);
    const json = await res.json();
    allSensorData.push(json);
  }

  // Сортувати за часом
  allSensorData.sort((a, b) => new Date(a.timestamp) - new Date(b.timestamp));

  populateDeviceFilter(allSensorData);
  updateView();
}

function populateDeviceFilter(data) {
  const select = document.getElementById("deviceFilter");
  const deviceIDs = [...new Set(data.map(entry => entry.deviceID))];

  for (const id of deviceIDs) {
    const option = document.createElement("option");
    option.value = id;
    option.textContent = id;
    select.appendChild(option);
  }

  select.addEventListener("change", () => {
    currentFilter = select.value;
    updateView();
  });
}

function updateView() {
  const filteredData = currentFilter === "all"
    ? allSensorData
    : allSensorData.filter(entry => entry.deviceID === currentFilter);

  displayTable(filteredData);
  buildCharts(filteredData);
}

function displayTable(data) {
  const tbody = document.querySelector("#dataTable tbody");
  tbody.innerHTML = "";

  for (const entry of data) {
    const row = document.createElement("tr");
    row.innerHTML = `
      <td>${entry.deviceID || 'Невідомий'}</td>
      <td>${entry.timestamp}</td>
      <td>${entry.temperature}</td>
      <td>${entry.humidity}</td>
      <td>${entry.CO} / ${entry.CH4}</td>
      <td>${entry.motion === true ? "Так" : "Ні"}</td>
    `;
    tbody.appendChild(row);
  }
}

window.addEventListener("DOMContentLoaded", loadSensorData);
