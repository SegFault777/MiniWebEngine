/* External script used by js_demo.html */
var clicks = 0;
function render() {
  document.getElementById("count").textContent = "Clicks: " + clicks;
}
document.addEventListener("DOMContentLoaded", function () {
  render();
  console.log("js_demo.js ready; regex test:", /^\d+$/.test("12345"));
});
setInterval(function () {
  var d = new Date();
  document.getElementById("clock").textContent = "UTC time " + d.toISOString().slice(11, 19);
}, 250);
