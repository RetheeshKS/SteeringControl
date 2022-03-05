#ifndef HTML_FORMATS_H
#define HTML_FORMATS_H
// tr:nth-child(even){background-color: #dddddd;}.center{padding: 70px 0;border: 3px solid green;text-align: center;}
const char index_html_p1[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    .center {
      padding: 70px 0;
      border: 3px solid green;
      text-align: center;
    }
  </style>
  </head><body>
  <form action="/post" method="post" id="form1">
    <div class="center">
    <input id="page_seq" name="page_seq" type="hidden" value="%d" />
    <input type="radio" id="r0" name="mode" style="display:none;" checked="true" value=0>
    <input type="radio" id="r1" name="mode" value=1> Custom             
    <input type="radio" id="r2" name="mode" value=2> Preset
    <br><br>
    <input type="submit" form="form1" name= "submit_btn" value="Add Keys >>">
    )rawliteral";
const char index_html_p2[] PROGMEM = R"rawliteral(
    </div>
    
  </form><br>
</body></html>)rawliteral";

char page1_alert[] PROGMEM = R"rawliteral(
                             <p style="color:red">!!!.Please select an option to proceed...</p>)rawliteral";

char add_key_fmt[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head><title>Register Steering Key</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
table{table-layout: fixed; width: 100%;}
//table{font-family: arial, sans-serif;border-collapse: collapse;width: 100%;}td,th{border: 1px solid #dddddd;text-align: left;padding: 8px;}
{background-color: #dddddd;}.center{padding: 70px 0;border: 3px solid green;text-align: center;}
</style>
</head><body onload="myFunction()">
  <script>
    var myVar;
    var pg;
    var ret;
    function myFunction(){
      cfm_val = "Scan Cancel";
      pg = document.getElementById("form1").prg_en.value;
      if (pg == "progress" || pg == "success"){
        //document.getElementById("form1").sca_bt.disabled = true;
        //pg = + document.getElementById("form1").prg_val.value;
        //if (pg <= 100){
           myVar = setTimeout(getProgress, 500);
          return;
       // }
      }else if(pg == "long_idle"){
        ret = confirm("Oops..No key press detected, do you want to retry ?");
      }else if(pg == "short_key"){
        ret = confirm("Oops..Scan process incomplete, key must be pressed and held for atleast 3 seconds.Do you wnat to retry ?");
      } else {
        if(pg == "dup_name"){
          alert("Oops..Key name is alrady in use, Please enter another name..");  
        }
        return;
      }
      if(ret){
        cfm_val = "Scan Key";
      }
      document.getElementById("form1").submit_btn.value = cfm_val;
      document.getElementById("form1").submit();
    }
    function getProgress() {
      if((+ document.getElementById("form1").prg_val.value) < 100){
        v = "Get Progress";
      } else {
        ret = confirm("Key Scan Success !, do you want to save the key?");
        if(ret){
          v = "Scan Done";
        } else {
          v = "Scan Cancel";
        }
      }
      document.getElementById("form1").submit_btn.value = v;
      document.getElementById("form1").submit();
      clearTimeout(myVar);
    }
    function onScan(val) {
      if(val == "Scan Key"){
        if(document.getElementById("form1").k_name.value == ""){
            alert("Please specify key name..");
            return;
        }
        document.getElementById("form1").submit_btn.value = val;
        document.getElementById("form1").submit();
      } else {
        document.getElementById("form1").submit_btn.value = "All keys Done";
        document.getElementById("form1").submit();
      }
    }
    function onRmv(val) {
      ret = confirm("Are you sure you want to remove this key.. ?");
      if(ret){
        document.getElementById("form1").submit_btn.value = "Remove";
        document.getElementById("form1").k_name.value = val;
        document.getElementById("form1").submit();
      }
    }
  </script>
  <form id="form1" action="/post" method="post">
    <input id="page_seq" name="page_seq" type="hidden" value="%d" />
    <input id="prg_en" name="prg_en" type="hidden" value="%s" />
    <input id="submit_btn" name="submit_btn" type="hidden" />
    <input id="prg_val" name="prg_val" type="hidden" value=%d />
    <div id="dv1" class="center" >
    Key Name: <input type="text" id="k_name" name="k_name" maxlength="10" %s />
    <input type="button" id="sca_bt" name="submit_bt" onclick="onScan(this.value);" value="Scan Key" %s/> 
    <br><br>
    <progress id="scan_pg" value=%d max="100" %s ></progress>
    %s
    </div>
    
    <table>
    <tr>
      <th>Key List</th>
      <th><input type="button" id="sca_bt" name="submit_bt" onclick="onScan(this.value);" value="Done"/></th>
    </tr>
    %s
    </table>
  </form><br>
  </body></html>)rawliteral";


char assign_key_fmt[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head><title>Register Steering Key</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
table{table-layout: fixed; width: 100%;}
//table{font-family: arial, sans-serif;border-collapse: collapse;width: 100%;}td,th{border: 1px solid #dddddd;text-align: left;padding: 8px;}
{background-color: #dddddd;}.center{padding: 70px 0;border: 3px solid green;text-align: center;}
</style>
</head><body onload="myFunction()">
  <script>
    var myVar;
    var pg;
    var ret;
    function myFunction(){
    }
    function onFinish(val) {
      document.getElementById("form1").submit_btn.value = val+' %s';
      document.getElementById("form1").submit();
    }
    function onAssign(val) {
      document.getElementById("form1").key_idx.value = val;
      document.getElementById("form1").submit_btn.value = "Assign %s";
      document.getElementById("form1").submit();
    }
    
    function onRmv(val) {
      ret = confirm("Are you sure you want to cancel this key.. ?");
      if(ret){
        document.getElementById("form1").submit_btn.value = "Cancel %s";
        document.getElementById("form1").key_idx.value = val;
        document.getElementById("form1").submit();
      }
    }
  </script>
  <form id="form1" action="/post" method="post">
    <input id="page_seq" name="page_seq" type="hidden" value="%d" />
    <input id="key_idx" name="key_idx" type="hidden" />
    <input id="submit_btn" name="submit_btn" type="hidden" />
    <div id="dv1" class="center" >
    <input type="button" id="sca_bt" name="submit_bt" onclick="onFinish(this.value);" value="Finish" /> 
    </div>
    
    <table>
    <tr>
      <th>Key List</th>
    </tr>
    %s
    </table>
  </form><br>
  </body></html>)rawliteral";

char define_macro_fmt_part1[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head><title>Register Steering Key</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
table{table-layout: fixed; width: 100%;}
//table{font-family: arial, sans-serif;border-collapse: collapse;width: 100%;}td,th{border: 1px solid #dddddd;text-align: left;padding: 8px;}
{background-color: #dddddd;}.center{padding: 70px 0;border: 3px solid green;text-align: center;}
</style>
</head><body onload="myFunction()">
  <script>
    var myVar;
    var pg;
    var ret;
    function myFunction(){
    }
    function onFinish(val) {
      document.getElementById("form1").submit_btn.value = val+' Def';
      document.getElementById("form1").submit();
    }
    function onAddStep() {
      document.getElementById("form1").key_name.value = document.getElementById("form1").drdn_steps.value;
      document.getElementById("form1").submit_btn.value = "Add Step";
      document.getElementById("form1").submit();
    }
    function onClearStep(val) {
      ret = confirm("Are you sure you want to clear this step.. ?");
      if(ret){
        document.getElementById("form1").submit_btn.value = "Clear Step";
        document.getElementById("form1").key_idx.value = val;
        document.getElementById("form1").submit();
      }
    }
  </script>
  <form id="form1" action="/post" method="post">
    <input id="page_seq" name="page_seq" type="hidden" value="%d" />
    <input id="key_idx" name="key_idx" type="hidden" />
    <input id="key_name" name="key_name" type="hidden" />
    <input id="submit_btn" name="submit_btn" type="hidden" />
    <div id="dv1" class="center" >
    <input type="button" id="sca_bt" name="submit_bt" onclick="onFinish(this.value);" value="Discard" />
    <input type="button" id="sca_bt" name="submit_bt" onclick="onFinish(this.value);" value="Save" />
    </div>
    
    <table>
    <tr>
      <th>%s : Command Sequence</th>
    </tr> )rawliteral";

char define_macro_fmt_part2[] PROGMEM = R"rawliteral(
</table>
  </form><br>
  </body></html>)rawliteral";
  
char map_key_fmt[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head><title>Register Steering Key</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
table{table-layout: fixed; width: 100%;}
//table{font-family: arial, sans-serif;border-collapse: collapse;width: 100%;}td,th{border: 1px solid #dddddd;text-align: left;padding: 8px;}
{background-color: #dddddd;}.center{padding: 70px 0;border: 3px solid green;text-align: center;}
</style>
</head><body onload="myFunction()">
  <script>
    var myVar;
    var pg;
    var ret;
    function myFunction(){
      
    }
    function onScan(val) {
      if(document.getElementById("form1").k_name.value == ""){
          alert("Please specify key name..");
          return;
      }
      document.getElementById("form1").submit_btn.value = val;
      document.getElementById("form1").submit();
    }
    
  </script>
  <form id="form1" action="/post" method="post">
    <input id="page_seq" name="page_seq" type="hidden" value="%d" />
    <input id="prg_en" name="prg_en" type="hidden" value="%s" />
    <input id="submit_btn" name="submit_btn" type="hidden" />
    <input id="prg_val" name="prg_val" type="hidden" value=%d />
    <div id="dv1" class="center" >
      <table>
      <tr>
        <th>Select HU Function</th><th>Select Steering Key</th>
      </tr>
      <tr>
        <td> 
          <select name="HU" id="HU">
            <option value="volvo">Volvo</option>
            <option value="saab">Saab</option>
            <option value="opel">Opel</option>
            <option value="audi">Audi</option>
          </select>
        </td>
        <td>
          <select name="ST" id="ST">
            <option value="volvo">Volvo</option>
            <option value="saab">Saab</option>
            <option value="opel">Opel</option>
            <option value="audi">Audi</option>
          </select>
        </td>
      </tr>
      </table>
    </div>
    <table>
    <tr>
      <th>HU Function</th><th>Steering Key</th>
    </tr>
    </table>
  </form><br>
  </body></html>)rawliteral";
#endif          
