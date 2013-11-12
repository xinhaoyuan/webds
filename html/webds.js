var di = {}

function copyToClipboard(s) {
    if (typeof s != "string") return;

    args = [sys("get_env", "WEBDS_PATH") + "/bin/clip-put", s];
    sys("cmd_output", args.length, args, "");

    args = [sys("get_env", "WEBDS_PATH") + "/bin/notify-desktop", s + " copied to clipboard"];
    sys("cmd_output", args.length, args, "");
}

di.change_sort_method = function() {
    var cm = $('#di-sort-by').text();
    if (cm == "name") {
        cm = "date";
    } else if (cm == "date") {
        cm = "type"
    } else cm = "name";

    $("#di-sort-by").text(cm);

    this.update();
}

di.update = function() {
    var cm = $('#di-sort-by').text();
    if (cm != "name" && cm != "date" && cm != "type") cm = "name";
    var args = [sys("get_env", "WEBDS_PATH") + "/bin/ls-by-" + cm, sys("get_env", "WEBDS_DESKTOP_PATH")];
    var r;

    try {
        r = sys("cmd_output", args.length, args, "")
        r = JSON.parse(r);
    } catch (e) { r = null; }

    if (r && r instanceof Array) {
        var l = $("#di-list").empty();
        for (var i in r) {
            l.append($("<div></div>").text(r[i].name).data("link", r[i].link));
        }
    }
}

$(document).ready(function() {
    if (!sys("load_plugin", "cmd", "libwebdsplugin-cmd.so"))
    {
        console.log("Cannot load the cmd plugin");
    }

    $("#di-list").delegate("div", "click", function(e) {
        copyToClipboard($(this).data("link"));
    });

    di.update();
});
