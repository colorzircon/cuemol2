// -*-Mode: C++;-*-
//
// Molecular surface calculation UI
//

( function () { try {

  ///////////////////////////
  // Initialization
  
  const pref = require("preferences-service");
  const util = require("util");
  
  var dlg = window.gDlgObj = new Object();
  dlg.mTgtSceID = window.arguments[0];
  dd("MapSharpDlg> TargetScene="+dlg.mTgtSceID);
  
  dlg.mObjBox = new cuemolui.ObjMenuList(
    "map-select-box", window,
    function (elem) {
      return cuemol.implIface(elem.type, "DensityMap");
    },
    cuemol.evtMgr.SEM_OBJECT);
  dlg.mObjBox._tgtSceID = dlg.mTgtSceID;

  window.addEventListener("load", function(){
    try {dlg.onLoad();} catch (e) {debug.exception(e);}
  }, false);
  
  ///////////////////////////
  // Event Methods

  dlg.onLoad = function ()
  {
    var that = this;
    
    this.mObjBox.addSelChanged(function(aEvent) {
      try { that.onObjBoxChanged(aEvent);}
      catch (e) { debug.exception(e); }
    });

    var nobjs = this.mObjBox.getItemCount();
    
    this.mBfac = document.getElementById("bfac-level");

    //alert("item count="+nobjs);
    if (nobjs==0) {
      // no mol obj to calc --> error?
      //this.mSurfName.disabled = true;
    }
    var obj = this.mObjBox.getSelectedObj();
  };
  
  dlg.onObjBoxChanged = function (aEvent)
  {
    dd("MapSharpDlg> ObjSelChg: "+aEvent.target.id);
  };

  dlg.onBfacChange = function (aEvent)
  {
    if ('isDragging' in aEvent && aEvent.isDragging)
      dd("Dragging");
    else
      dd("Changed");

    this.preview();
  };

  dlg.preview = function ()
  {
    var tgtobj = this.mObjBox.getSelectedObj();
    if (tgtobj==null)
      return;

    ////
    // Get bfactor value
    var bfac = parseFloat(document.getElementById("bfac-value").value);
    if (bfac==NaN)
      return; // ERROR

    tgtobj.sharpenMapPreview(bfac);
  };

  dlg.onDialogCancel = function (event)
  {
    var tgtobj = this.mObjBox.getSelectedObj();
    if (tgtobj==null)
      return;

    // Reset the preview map
    tgtobj.sharpenMapPreview(0.0);
    return;
  };

  dlg.onDialogAccept = function (event)
  {
    var tgtobj = this.mObjBox.getSelectedObj();
    if (tgtobj==null)
      return;

    // test -- reset
    tgtobj.sharpenMapPreview(0.0);
    return;
    

    // EDIT TXN START //
    //scene.startUndoTxn("Create mol surface");

    try {
      tgtobj.sharpenMapPreview(bfac);
    }
    catch (e) {
      dd("Error: "+e);
      debug.exception(e);
      return;
    }

    // EDIT TXN END //
    // scene.commitUndoTxn();
  }

} catch (e) {debug.exception(e);} } )();
