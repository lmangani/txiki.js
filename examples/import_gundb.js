import * as Gun from 'https://cdn.jsdelivr.net/npm/gun/gun.js';

var gun = Gun();

gun.get('quv').put({
  name: "quickJS",
  powers: "LibUV"
});

gun.get('quv').on(function(data, key){
  console.log("update:", data);
});
