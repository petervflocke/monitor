const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang=pl>
<head>
  <meta charset="UTF-8">
  <title>Monitor</title>
<link rel="stylesheet" href="./style.css">
<script src="https://kit.fontawesome.com/4aef49e2ee.js" crossorigin="anonymous"></script>

<style>
body {
  font-family: "Helvetica Neue", Helvetica, Arial;
  font-size: 60px;
  line-height: 80px;
  font-weight: 600;
  -webkit-font-smoothing: antialiased;
  font-smoothing: antialiased;
  background: #2b2b2b;
}

.wrap {
  margin: 0 auto;
  padding: 30px;
  max-width: 1080px;
}

.tab {
  margin: 0 0 40px 0;
  width: 100%;
  display: table;
}

.row {
  display: table-row;
  background: #f6f6f6;
}
.row:nth-of-type(odd) {
  background: #e9e9e9;
}
.row.header {
  font-weight: 550;
  color: #ffffff;
  background: #ea6153;
}
.row.g {
  background: #27ae60;
}
.row.b {
  background: #2980b9;
}
.row.y {
  background: #DAA520;
}
.row.s {
  background: black;
}

.cell {
  padding: 6px 12px;
  display: table-cell;
  text-align: left; 
}
.cell2 {
  display: table-cell;
  text-align: center; 
  padding-right:25px
}
.cell2.g {
  color:#27ae60
}
.cell2.b {
  color:#2980b9
}
.cell2.y {
  color:#DAA520
}

.v {
  display:inline-block
} 
</style>
</head>
<body>
<div class=wrap>

  <div class=tab>
    <div class="row header s">
      <div id=XC1 style=color:white>
        <div class=cell><i class="fas fa-clock"></i></div>      
        <div id=clock class=cell>??:??:?? &nbsp; ??.??.????</div>
      </div>
    </div>
  </div>  

  <div class=tab>
    <div class="row header g">
      <div class=cell>Dom&nbsp;</div>
      <div class=cell2>@</div>
      <div id=time0 class=cell>??:?? &nbsp; ??.??.??</div>         
    </div>

    <div class=row>
      <div class=cell>Temp.</div>
      <div class="cell2 g"><i class="fa fa-thermometer-half"></i></div>      
      <div class=cell><div class=v id=temp0>??.??</div>°C</div>
    </div>

    <div class=row>
        <div class=cell>Wilg.</div>
        <div class="cell2 g"><i class="fas fa-tint"></i></div>              
        <div class=cell><div class=v id="hum0">??</div>%</div>
      </div>

      <div class=row>
        <div class=cell>Cisn.</div>
        <div class="cell2 g"><i class="fas fa-tachometer-alt"></i></div>              
        <div class=cell><div id="pres0" class=v>????</div>HPa</div>
      </div>

      <div class=row>
        <div class=cell>Bat.</div>
        <div class="cell2 g"><i class="fas fa-battery-three-quarters fa-sm"></i></div>
        <div class=cell><div id="bat0" class=v>??.?</div>V</div>
      </div>      
  </div>

  <div class=tab>
    <div class="row header b">
      <div class=cell>Ogród</div>
      <div class=cell2>@</div>
      <div id=time1 class=cell>??:?? &nbsp; ??.??.??</div>         
    </div>
    
    <div class=row>
      <div class=cell>Temp.</div>
      <div class="cell2 b"><i class="fa fa-thermometer-half"></i></div>      
      <div class=cell><div id=temp1 class=v>25,17</div>°C</div>
    </div>

    <div class=row>
        <div class=cell>Wilg.</div>
        <div class="cell2 b"><i class="fas fa-tint"></i></div>              
        <div class=cell><div id=hum1 class=v>57</div>%</div>
    </div>

    <div class=row>
      <div class=cell>Cisn.</div>
      <div class="cell2 b"><i class="fas fa-tachometer-alt"></i></div>
      <div class=cell><div id=pres1 class=v>1001</div>HPa</div>
    </div>

    <div class=row>
      <div class=cell>Bat.</div>
      <div class="cell2 b"><i class="fas fa-battery-three-quarters fa-sm"></i></div>
      <div class=cell><div id=bat1 class=v>4,89</div>V</div>
    </div>
  </div>
  
  <div class=tab>
    
    <div class="row header y">
      <div class=cell>Dziura</div>
      <div class=cell2>@</div>
      <div id=time2 class=cell>12:59 &nbsp; 31.12.20</div>         
    </div>
    
    <div class=row>
      <div class=cell>Temp.</div>
      <div class="cell2 y"><i class="fa fa-thermometer-half"></i></div>      
      <div class=cell><div id=temp2 class=v>25,17</div>°C</div>
    </div>

    <div class=row>
        <div class=cell>Wilg.</div>
        <div class="cell2 y"><i class="fas fa-tint"></i></div>              
        <div class=cell><div id=hum2 class=v>57</div>%</div>
      </div>

      <div class=row>
        <div class=cell>Cisn.</div>
        <div class="cell2 y"><i class="fas fa-tachometer-alt"></i></div>              
        <div class=cell><div id=pres2 class=v>1001</div>HPa</div>
      </div>
  </div>
</div>

<script>
function listen() {
  var source = new EventSource("/events");
  source.addEventListener('data', function(e) {
    // console.log("myevent", e.data);
      dic = JSON.parse(e.data);
      for (var key in dic) {
          var target = document.getElementById(key);
          if (target != undefined) {
            if (key.substring(0,2) != "XC") {
              target.innerHTML = dic[key];
            } else {
              target.style.color = dic[key];
            }
          }
      }
  }, false);
}
listen();
</script>

</body>
</html>
)rawliteral";