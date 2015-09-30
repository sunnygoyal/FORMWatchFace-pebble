Pebble.addEventListener("showConfiguration", function(_event) {
  var url = (Pebble.getActiveWatchInfo && (Pebble.getActiveWatchInfo().platform == "basalt")) ? "http://sunnygoyal.github.io/FORMWatchFace-pebble/settings/?" : "http://sunnygoyal.github.io/FORMWatchFace-pebble/settings/aplite.html?";
  var keys = ["color1", "color2", "color3", "tricolor", "showdate", "format12"];
  for (var i = 0; i < keys.length; i++) {
    if (localStorage[keys[i]]) {
      url += encodeURIComponent(keys[i]) + "=" + encodeURIComponent(localStorage[keys[i]]) + "&";
    }
  }
	Pebble.openURL(url);
});

Pebble.addEventListener("webviewclosed", function(e) {
	var configData = JSON.parse(decodeURIComponent(e.response));
  if (configData.showdate === undefined) return;
  if (Pebble.getActiveWatchInfo && (Pebble.getActiveWatchInfo().platform == "basalt")) {
    Pebble.sendAppMessage({
      color1: parseInt(configData.color1, 16),
      color2: parseInt(configData.color2, 16),
      color3: parseInt(configData.color3, 16),
      showdate: configData.showdate,
      format12: configData.format12

      }, function() {
        console.log('Send successful!');
        localStorage.format12 = configData.format12;
        localStorage.showdate = configData.showdate;
        localStorage.color1 = configData.color1;
        localStorage.color2 = configData.color2;
        localStorage.color3 = configData.color3;
      }, function() {
        console.log('Send failed!');
      });
  } else {
    Pebble.sendAppMessage({
      showdate: configData.showdate,
      tricolor: configData.tricolor,
      format12: configData.format12
      }, function() {
        console.log('Send successful!');
        localStorage.format12 = configData.format12;
        localStorage.showdate = configData.showdate;
        localStorage.tricolor = configData.tricolor;
      }, function() {
        console.log('Send failed!');
      });
  }
});