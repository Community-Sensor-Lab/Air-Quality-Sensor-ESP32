
/****************************************************************************
* Format sheet headers 
****************************************************************************/

// Current Sheet
var SS = SpreadsheetApp.openById('<SHEET ID>');
var sheet = SS.getSheetByName('Sheet1');

function onOpen() { 
  const headerRow = [
    "Google datetime", "Sensor datetime", "CO2_scd41", "T_scd41", "RH_scd41", 
    "T_bme280", "P_bme280", "RH_bme280", "dvbat(mV)", "status", 
    "mC_Pm1_sen5x", "mC_Pm2_sen5x", "mC_Pm4_sen5x", "mC_Pm10_sen5x", 
    "nC_Pm0_5_sen5x", "nC_Pm1_sen5x", "nC_Pm2_sen5x", "nC_Pm4_sen5x", "nC_Pm10_sen5x", 
    "typPartSize_sen5x", "ambientRH_sen5x", "ambientTemp_sen5x", 
    "vocIndex_sen5x", "noxIndex_sen5x"
  ];

  sheet.getRange(1, 1, 1, headerRow.length).setValues([headerRow]);
}

/****************************************************************************
* The doPost(e) triggers when a client calls a post request on theApp Script 
* using the Web App Url
*****************************************************************************/
// Handle POST request
function doPost(e) {
  let parsedData;
  try {
    parsedData = JSON.parse(e.postData.contents);
  }
  catch(f){
    return ContentService.createTextOutput("Error in parsing request body: " + f.message);
  }

  if (parsedData !== undefined){
    switch (parsedData.command) {
      case "appendRow":
        var dataArr = parsedData.values.split(","); 
        // Row format - "datetime","O3 ppbv","cell temp C","press mbar","flow cc/min"  
        var d = new Date(); 
        dformat = [d.getFullYear(), d.getMonth() + 1, d.getDate()].join('/') + ' ' + [d.getHours(), d.getMinutes(), d.getSeconds()].join(':');
        dataArr.unshift(dformat);
        // Appends to the last row
        sheet.appendRow(dataArr); 
        // Save to sheets
        SpreadsheetApp.flush();
        break;
    }
      return ContentService.createTextOutput("Success");
    } // endif (parsedData !== undefined)
  else {
      return ContentService.createTextOutput("Error!   Request body empty or in incorrect format.");
    }   
}

