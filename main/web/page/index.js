"use strict";

function startUpload() {
    document.getElementById("status").innerHTML = "Downloading...";
    var otafile = document.getElementById("otafile").files;
    var file = otafile[0];
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
        if (xhr.readyState == 4) {
            if (xhr.status == 200) {
                // document.open();
                // document.write(xhr.responseText);
                document.getElementById("status").innerHTML = xhr.responseText;
                // document.close();
            } else if (xhr.status == 0) {
                alert("Server closed the connection");
                location.reload();
            } else {
                alert(xhr.status + " Error!\n" + xhr.responseText);
                location.reload();
            }
        }
    };
    xhr.upload.onprogress = function (e) { document.getElementById("status").innerHTML = ((e.loaded / e.total * 100).toFixed(0)) + "%"; };
    xhr.open("POST", "/update", true);
    xhr.send(file);
}

window.startUpload = startUpload;