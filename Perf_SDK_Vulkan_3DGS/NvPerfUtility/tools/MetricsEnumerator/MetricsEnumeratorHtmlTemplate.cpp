/*
* Copyright 2014-2025 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <string>

namespace nv { namespace perf { namespace tool {

    extern const std::string HtmlTemplate = R"(
<html>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <head>
    <title>MetricsEnumeration</title>
    <style id="ReportStyle">
      body {
        background-color: #202020;
        font-family: Verdana, Geneva, sans-serif;
        margin: 0;
        padding: 0;
      }
      .container {
          max-width: 1600px; 
          margin: 0 auto;   
          padding-top: 40px; 
          padding: 20px;    
          background-color: #353535;
          border-radius: 15px;
          box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
      }
      .titlearea {
        display: flex;
        align-items: center;
        color: white;
        font-family: verdana;
        position: relative;
        top: 40px; 
      }
      .titlebar {
        margin-left: 0;
        margin-right: auto;
        margin-top: auto; 
        display: flex;
        align-items: left;
      }
      .logo {
        margin-top: 5px; 
      }
      .title {
        font-size: 28px;
        margin-left: 10px;
      }
      .navbar {
        width: 100%;
        background-color: #303030;
        overflow: hidden;
        position: fixed;
        top: 0;
        z-index: 1000;
        display: flex;
        justify-content: center;
        flex-wrap: wrap;
      }
      .navbar a {
        float: left;
        padding: 12px;
        color: white;
        text-decoration: none;
        font-size: 12px;
        text-align: center;
        align-items: center;
        white-space: nowrap; 
      }
      .navbar a:hover,
      .navbar a.active { 
        background-color: #ddd;
        color: black;
      }
      .section {
        border-radius: 15px;
        padding: 10px;
        background: #FFFFFF;
        margin: 10px;
        margin-top: 60px; 
      }
      .section {
        border-radius: 15px;
        padding: 10px;
        background: #FFFFFF;
        margin: 10px;
        margin-top: 60px; 
      }
      .highlight { 
        /* border: 5px solid #447607;  */
        background-color: rgba(190, 201, 193, 0.671);
      }
      summary {
        display: block;
        padding: 2px 6px;
        color: white;
        background-color: #598800;
        border-radius: 5px;
        box-shadow: 1px 1px 2px black;
        cursor: pointer;
        transition: background-color 0.2s;
        margin-bottom: 5px;
      }
      details {
        display: block;
      }
      details[open] > summary {
        background-color: #77B900;
      }
      details > summary:only-child::-webkit-details-marker {
        display: none;
      }
      details > details {
        margin-left: 22px;
      }
      .value {
        color: #228B22;
        text-align: right;
      }
      table {
        width: 100%;
        border-collapse: collapse;
        table-layout: fixed;
        margin-top: 5px;
        margin-bottom: 10px;
      }
      th, td {
        border: 1px solid black;
        padding: 8px;
        text-align: left;
        box-sizing: border-box; 
        white-space: normal;
        word-wrap: break-word;
      }
      th {
        background-color: #f2f2f2;
      }
    </style>
    <script type="text/JavaScript">
      function createTableForItems(items) {
        let table = document.createElement('table');

        let colgroup = document.createElement('colgroup');
        let col1 = document.createElement('col');
        col1.style.width = '37%'; 
        let col2 = document.createElement('col');
        col2.style.width = '8%';
        let col3 = document.createElement('col');
        col3.style.width = '42%'; 
        let col4 = document.createElement('col');
        col4.style.width = '13%'; 
        colgroup.appendChild(col1);
        colgroup.appendChild(col2);
        colgroup.appendChild(col3);
        colgroup.appendChild(col4);
        table.appendChild(colgroup); 

        let headerRow = table.insertRow(-1);
        let headers = ['Name', 'Type', 'Description', 'Dimensional Units'];
        headers.forEach(headerText => {
          let headerCell = document.createElement('th');
          headerCell.textContent = headerText;
          headerRow.appendChild(headerCell);
        });

        items.sort((a,b) => a.name.localeCompare(b.name));
        items.forEach(item => {
          let row = table.insertRow(-1);
          row.insertCell(-1).textContent = item.name;
          row.insertCell(-1).textContent = item.type;
          row.insertCell(-1).textContent = item.description;
          row.insertCell(-1).textContent = item.dimensional_units;
        });
        return table;
      }
      function appendNodeRecursively(obj, domNode) {
        const colors = {
            "GR Engine":"#B45F06",
            "RASTER":   "#434343",
            "RTCORE":   "#274E13",
            "PROP":     "#741B47",
            "CROP":     "#741B47",
            "ROP":      "#741B47",
            "ZROP":     "#741B47",
            "PES":      "#351C75",
            "VPC":      "#351C75",
            "VAF":      "#7F6000",
            "SM":       "#7F6000",
            "SMSP":     "#7F6000",
            "L1TEX":    "#7F6000",
            "PDA":      "#7F6000",
            "FE":       "#B45F06",
            "PCIe":     "#1155CC",
            "NVLINK":   "#1155CC",
            "L2 Cache": "#1155CC",
            "FBPA":     "#1155CC",
            "DRAM":     "#1155CC",
            "DRAMC":    "#1155CC",
            "SYS":      "#1155CC",
            "TPC":      "#CFE2F3",
            "GPC":      "#EAD1DC",
            "ZCULL":    "#434343",
        };
        let keys = Object.keys(obj).sort();
        keys.forEach(function(key) {
            if (key !== "chipName" && obj.hasOwnProperty(key)) {
                let detailsNode = document.createElement('details');
                let summary = document.createElement('summary');
                summary.textContent = key;
                summary.id = key.toLowerCase().replace(/ /g, '-'); // Sanitize the key for use as an ID
                summary.style.backgroundColor = colors[key] || "#77B900";
                detailsNode.appendChild(summary);
                detailsNode.open = true;
                let items = obj[key];
                if (Array.isArray(items)) {
                    let table = createTableForItems(items);
                    detailsNode.appendChild(table);
                }
                domNode.appendChild(detailsNode);
            }
        });
      }
      function createNavBar(categories) {
        let navbar = document.createElement('div');
        navbar.className = 'navbar';
        let keys = Object.keys(categories).sort();
        keys.forEach(function(key) {
          if(key != "chipName"){
            let sanitizedKey = key.replace(/ /g, '-');
            let a = document.createElement('a');
            a.textContent = key;
            a.href = '#' + sanitizedKey.toLowerCase();
            a.onclick = function(event) {
              event.preventDefault(); 
              const targetId = this.getAttribute('href');
              const targetElement = document.querySelector(targetId);
              if (targetElement) {
                const navbarHeight = document.querySelector('.navbar').offsetHeight;
                const yOffset = -navbarHeight; 
                const y = targetElement.getBoundingClientRect().top + window.pageYOffset + yOffset - 40;
                window.scrollTo({top: y, behavior: 'smooth'}); 
              }
              highlightSection(key); 
            };
            navbar.appendChild(a);
          }
        });
        document.body.insertBefore(navbar, document.body.firstChild);
      }
      function highlightSection(id) {
        document.querySelectorAll('.highlight').forEach(function(highlighted) {
          highlighted.classList.remove('highlight');
        });
        const targetId = id.toLowerCase().replace(/ /g, '-');
        const targetElement = document.getElementById(targetId);
        if (targetElement) {
          targetElement.parentNode.classList.add('highlight');
          console.log("Highlighted section:", id); 
        } else {
          console.log("Failed to find the section:", id); 
        }
      }
      function adjustPadding() {
        const navbarHeight = document.querySelector('.navbar').offsetHeight;
        document.querySelector('.container').style.paddingTop = navbarHeight + 'px';
      }
      function onBodyLoaded() {
        let main = document.getElementById('main');
        appendNodeRecursively(g_json, main);
        createNavBar(g_json);
        if (g_json && g_json.chipName) {
          document.getElementById('titlebar_text').textContent = "Nsight Perf SDK Metrics - " + g_json.chipName;
        }
        adjustPadding();
        window.addEventListener('resize', adjustPadding);
    }
    </script>
  </head>
  <body onload="onBodyLoaded() ">
    <noscript>
      <p>Enable javascript to see report contents</p>
    </noscript>
    <div class="container">
        <div class="titlearea">
          <div class="titlebar">
            <div class="logo" aria-label="Company Logo">
              <svg width="100%" height="30" viewBox="0 0 30 18" fill="none" xmlns="http://www.w3.org/2000/svg">
                  <path d="M9.79534 5.31093V3.70317C13.839 3.10878 17.5269 7.45469 17.3659 7.457C17.4065 7.63929 13.2055 12.8441 9.79534 11.5767V6.70148C11.4661 6.90826 11.802 7.66442 12.8068 9.37989L15.0409 7.45001C15.0365 7.18791 12.4148 4.93506 9.79534 5.31093ZM9.79534 0V2.40149C15.4866 1.72074 20.2935 7.4054 20.1152 7.38817C20.2415 7.48 14.2092 14.0948 9.79534 12.8441V14.3286C14.4012 14.8263 18.0854 12.3236 21.2889 9.44804C21.7912 9.86048 23.8495 10.8637 24.2728 11.3035C20.995 13.9746 14.0423 16.0668 9.79534 15.7124V17.7986H26.2529V0L9.79534 0ZM9.79534 11.5767V12.8441C5.79044 12.1126 4.32025 7.81787 4.32025 7.81787C4.32025 7.81787 6.56323 5.59204 9.79534 5.31093V6.70148C8.12332 6.48878 6.24693 8.17117 6.24693 8.17117C6.24693 8.17117 6.90118 10.7496 9.79534 11.5767ZM2.68227 7.66254C2.68227 7.66254 5.05584 4.07418 9.79534 3.70317V2.40149C4.54589 2.8331 0 7.38817 0 7.38817C0 7.38817 2.57452 15.0143 9.79534 15.7124V14.3286C4.49657 13.6455 2.68227 7.66254 2.68227 7.66254Z" fill="#74B71B"></path>
              </svg>
          </div>
          <span class="title" id="titlebar_text">Nsight Perf SDK Metrics</span>
          </div>
        </div>
      <div class="section" id="main">
      </div>
    </div>
    <script>
      g_json = /***JSON_DATA_HERE***/;
    </script>
  </body>
</html>
)";

}}} // nv::perf::tool
