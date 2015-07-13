if (!Module) Module = (typeof Module !== 'undefined' ? Module : null) || {};

// Disable image and audio decoding
Module.noImageDecoding = true;
Module.noAudioDecoding = true;

console.log("Step1");

Module['print'] = function(text) {
  console.log(text) 
};

Module['preRun'] = function() {
  // FS['mkdir']('/vfs');
  // FS['mount'](NODEFS, { root: '.' }, '/vfs');
  // FS['chdir']('/vfs');
};
