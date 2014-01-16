var launcherCurrentArgument;
var launcherHead;
var launcherCurrentArgumentRangeStart;
var launcherBusy = false;
var headerSmartType = "Cmd";
var regexEmail = /^(([^<>()[\]\\.,;:\s@\"]+(\.[^<>()[\]\\.,;:\s@\"]+)*)|(\".+\"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
var regexWeb = /^((http|https):\/\/)?[\w-]+(\.[\w-]+)+([\w.,@?^=%&amp;:\/~+#-]*[\w@?^=%&amp;\/~+#-])?/;
var regexWebExclude = /[^\/]+(\-?([0-9]+\.)*[0-9]+|\.sh|\.py|\.bin)$/;
// var regexWeb2 = /[-a-zA-Z0-9@:%_\+.~#?&//=]{2,256}\.[a-z]{2,4}\b(\/[-a-zA-Z0-9@:%_\+.~#?&//=]*)?/gi;
var headerCache = "";
var updateIntervalV;
var rootContainer;
var message_callback;

var currentCompletionIndex = null;

var oldHeight = null;
var oldWidth = null;

function HideAndReset()
{
    clearInterval(updateIntervalV);
    updateInterval();
    sys.call("hide_window");
}

function Show() {
    sys.call("show_window");
    sys.call("grab_focus");
    updateIntervalV = setInterval(updateInterval, 200);
    $(".launcher-argument[contenteditable=true]").focus();
    launcherBusy = false;
}

function updateInterval()
{
    // smart type indicator

    updateHeaderSmartType();
    if (headerSmartType == "Cmd") {
        $("#launcher-line-container").attr("smart-type", "Cmd")
    } else if (headerSmartType == "JSEval") {
        $("#launcher-line-container").attr("smart-type", "JSEval");
    } else if (headerSmartType == "Email") {
        $("#launcher-line-container").attr("smart-type", "Email");
    } else if (headerSmartType == "Web") {
        $("#launcher-line-container").attr("smart-type", "Web");
    }

    var width = rootContainer.offsetWidth;
    var height = rootContainer.offsetHeight;

    if (width != oldWidth || height != oldHeight)
    {
        oldWidth = width;
        oldHeight = height;
        sys.call("resize_window", width, height);
    }
}

function SetCurrentArgumentText(txt)
{
    launcherCurrentArgument.text(txt);
    // Move the cursor to the end
    var dom = launcherCurrentArgument.get(0);
    if (document.createRange)
    {
        range = document.createRange();
        range.selectNodeContents(dom);
        range.collapse(false);
        selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(range);
    }
    else if (document.selection)
    { 
        range = document.body.createTextRange();
        range.moveToElementText(dom);
        range.collapse(false);
        range.select();
    }
}

function ArgumentIsCompleted(arg, pos)
{
    var c = "";

    for (var i = 0; i < pos && i < arg.length; ++ i)
    {
        if (arg.charAt(i) == '"' && c == "")
            c = '"';
        else if (arg.charAt(i) == '"' && c == '"')
            c = ""
        else if (arg.charAt(i) == "'" && c == "")
            c = "'";
        else if (arg.charAt(i) == "'" && c == "'")
            c = "";
    }
    return c == "";
}

function updateHeaderSmartType()
{
    launcherHead = $($(launcherCurrentArgument).parent().children()[0]);
    var header = launcherHead.text();

    if (header == headerCache) return;
    headerCache = header;

    if (headerCache == "#") headerSmartType = "JSEval";
    else if (regexEmail.test(headerCache)) headerSmartType = "Email";
    else if (regexWeb.test(headerCache) &&
             !regexWebExclude.test(headerCache))
        headerSmartType = "Web";
    // else if (regexWeb2.test(headerCache)) headerSmartType = "Web";
    else headerSmartType = "Cmd";
}

function LauncherDoCompletion()
{
    // Assume that completion won't change headerSmartType
    updateHeaderSmartType();
    LauncherCompletionClean();

    var container = $("#launcher-completion-container");
    var comp, dcomp = null;
    
    if (launcherCurrentArgument.index() == 1 && headerSmartType == "JSEval")
    {
        var result;
        var error = false;
        try
        { result = String(eval(launcherCurrentArgument.text())); }
        catch (e)
        { result = String(e); error = true; }
        container.text(result).addClass("show");
        return;
    }

    if (launcherCurrentArgument.index() == 0)
    {
        if (headerSmartType == "Cmd")
            comp = sys.call("dirlist_prog", launcherCurrentArgument.text());
        else return;

        if (comp != null && comp.length > 0) {
            dcomp = comp[0];
            comp.shift();
        }
    }
    else if (headerSmartType == "Cmd" &&
             launcherHead.text() == "!" &&
             launcherCurrentArgument.index() == 1)
    {
        comp = sys.call("dirlist_prog", launcherCurrentArgument.text());
        if (comp != null && comp.length > 0) {
            dcomp = comp[0];
            comp.shift();
        }
    }
    else 
    {
        if (headerSmartType == "Cmd")
            // XXX
            // comp = sys.call("dirlist_dir", launcherCurrentArgument.text());
            return;
        else return;

        if (comp != null && comp.length > 0)
        {
            dcomp = "";
            var s = comp[0];
            var e = comp[comp.length - 1];
            var i = 0;
            while (i < s.length && i < e.length && s.charAt(i) == e.charAt(i))
            {
                dcomp += s.charAt(i);
                ++ i;
            }
        }
    }


    if (comp != null && comp.length > 0) {
        for (i = 0; i < comp.length; ++ i)
        {
            container.append("<div class='launcher-completion'>" + comp[i] + "</div>");
        }
        container.addClass("show");
    }
    
    if (dcomp != null) {
        SetCurrentArgumentText(dcomp);
    }
}

function LauncherClean()
{
    $("#launcher-line-container").attr("smart-type", "Cmd");
    var container = $("#launcher-arguments-container");
    container.empty();
    container.append("<div class='launcher-argument' contenteditable='true'></div>");
    headerSmartType = "Cmd";
}

function LauncherCompletionClean()
{
    var compContainer = $("#launcher-completion-container");
    currentCompletionIndex = null;
    compContainer.empty().removeClass("show");
}

function MoveCompletion(index)
{
    var containerDOM = document.getElementById('launcher-completion-container');
    if (index < 0 || index >= containerDOM.childNodes.length) return;
    var now = $(containerDOM.childNodes[index]);
    if (!now.hasClass("launcher-completion")) return;

    if (currentCompletionIndex != null)
        $(containerDOM.childNodes[currentCompletionIndex]).removeClass("selected");
    currentCompletionIndex = index;
    now.addClass("selected");
    var top = now.get(0).offsetTop - containerDOM.offsetTop - (containerDOM.clientHeight - now.get(0).offsetHeight) / 2;
    containerDOM.scrollTop = top;
    SetCurrentArgumentText(now.text());
}

function MovePrevCompletion(count)
{
    var containerDOM = document.getElementById('launcher-completion-container');
    var index;
    if (currentCompletionIndex == null)
    {
        index = containerDOM.childNodes.length - 1;
    }
    else
    {
        index = currentCompletionIndex - count;
        if (index < 0) 
            index = currentCompletionIndex == 0 ? 
            containerDOM.childNodes.length - 1 : 0;
    }
    MoveCompletion(index);
}

function MoveNextCompletion(count)
{
    var containerDOM = document.getElementById('launcher-completion-container');
    var index
    if (currentCompletionIndex == null)
    {
        index = 0;
    }
    else
    {
        var index = currentCompletionIndex + count;
        if (index >= containerDOM.childNodes.length) 
            index = currentCompletionIndex == containerDOM.childNodes.length - 1 ? 
            0 : containerDOM.childNodes.length - 1;
    }
    MoveCompletion(index);
}

function LauncherFinish(withCtrl)
{
    if (launcherBusy) return;
    launcherBusy = true;

    var container = $("#launcher-arguments-container");
    var children = container.children(".launcher-argument");
    var result = [];
    for (var i = 0; i < children.size(); ++ i) {
        var str = $(children[i]).text();
        if (str != "")
            result.push($(children[i]).text());
    }

    updateHeaderSmartType();
    var submitType = headerSmartType;

    LauncherClean();
    LauncherCompletionClean();
    HideAndReset();

    if (submitType == "Cmd")
    {
        if (result[0] == "!")
            result.shift();
        if (withCtrl)
        {
            result.unshift("-e")
            result.unshift("x-terminal-emulator")
        }
        sys.call("cmd", result.length, result);
    }
    else if (submitType == "Web")
    {
        result.unshift("x-www-browser");
        sys.call("cmd", result.length, result);
    }
    else if (submitType == "Email")
    {
    }
}

function LauncherMoveTo(last, current)
{
    if (last.text() != "")
        last.attr("contenteditable", false);
    else if (last.index() != current.index()) last.remove();
    current.attr("contenteditable", true).focus();

    LauncherCompletionClean();

    // Selects all text
    var sel, range;
    if (window.getSelection && document.createRange) {
        range = document.createRange();
        range.selectNodeContents(current[0]);
        sel = window.getSelection();
        sel.removeAllRanges();
        sel.addRange(range);
    } else if (document.body.createTextRange) {
        range = document.body.createTextRange();
        range.moveToElementText(current[0]);
        range.select();
   }
}

function LauncherMoveForward(cycle)
{
    var container = $("#launcher-arguments-container");
    var current = launcherCurrentArgument;
    var last = current;
    var children = container.children();

    if (current.index() == children.size() - 1) {
        if (cycle) {
            current = $(children[0]);
        } else if (current.text() == "") {
            return;
        } else container.append(current = $("<div class='launcher-argument'></div>"));
    }
    else current = $(children[current.index() + 1]);

    LauncherMoveTo(last, current);
}

function LauncherMoveBackward(cycle)
{
    var container = $("#launcher-arguments-container");
    var current = launcherCurrentArgument;
    var last = current;
    var children = container.children();

    if (current.index() <= 0) {
        if (cycle) { 
            current = $(children[children.size() - 1]);
        } else return;
    } else current = $(children[current.index() - 1]);

    LauncherMoveTo(last, current);
}

function LauncherKeyPressedInCurrentArgument(event)
{
    updateHeaderSmartType();
    var bypass = true;
    // Process special key here
    if (event.which == 13) {
        // enter
        (window.getSelection().anchorNode);
        if (event.ctrlKey) {
            bypass = false;
            if (headerSmartType != "JSEval")
                LauncherFinish(true);
        } else {            
            bypass = false;

            if (headerSmartType == "JSEval" && launcherCurrentArgument.index() == 1) {
                bypass = true;
            } else if (event.shiftKey) {
                bypass = true;
            } else { 
                // Finish input and send
                LauncherFinish(false);
            }
            
        }
    } else if (event.which == 33) {
        // page up
        bypass = false;
        MovePrevCompletion(8);
    } else if (event.which == 34) {
        // page down
        bypass = false;
        MoveNextCompletion(8);
    } else if (event.which == 37) {
        // left
        if (event.ctrlKey && event.altKey) {
            bypass = false;
            LauncherMoveBackward(1);
        }
    } else if (event.which == 39) {
        // right
        if (event.ctrlKey && event.altKey) {
            bypass = false;
            LauncherMoveForward(1);
        }
    } else if (event.which == 38) {
        // up
        if (headerSmartType != "JSEval") {
            bypass = false;
            MovePrevCompletion(1);
        }
    } else if (event.which == 40) {
        // down
        if (headerSmartType != "JSEval") {
            bypass = false;
            MoveNextCompletion(1);
        }
    } else if (event.which == 8) {
        // backspace
        if ($(".launcher-argument[contenteditable=true]").text() == "")
        {
            bypass = false;
            LauncherMoveBackward(0);
        }
    } else if (event.which == 9) {
        // tab
        bypass = false;
        if (ArgumentIsCompleted(launcherCurrentArgument.text(), 0))
            LauncherDoCompletion();
    } else if (event.which == 32) {
        // space
        if (ArgumentIsCompleted(launcherCurrentArgument.text(), launcherCurrentArgumentRangeStart) &&
            (launcherCurrentArgument.index() == launcherHead.index() || 
             (headerSmartType != "JSEval")))
        {
            bypass = false;
            LauncherMoveForward(0);
        }
        else bypass = true;
    }
    
    return bypass;
}

$(document).ready(function() {
    if (!sys.call("load_plugin", "cmd", "libwebdsplugin-cmd.so")) {
        console.log("Cannot load the cmd plugin");
    }

    if (!sys.call("load_plugin", "dirlist", "libwebdsplugin-dirlist.so")) {
        console.log("Cannot load the dirlist plugin");
    }

    if (!sys.call("load_plugin", "messager", "libwebdsplugin-messager.so")) {
        console.log("Cannot load the messager plguin");
    }

    sys.call("messager_register_listener", "/tmp/webds_launcher", "message_callback");

    $("body").click(function() {
        sys.call("grab_focus");
    });

    $(document).bind("keydown", function(event) {
        if (event.which == 27)
        {
            // ESC
            launcherBusy = true;
            LauncherClean();
            LauncherCompletionClean();
            HideAndReset();
            return false;
        }
        // To fix the wired focus condition
        var f = window.getSelection().anchorNode;
        launcherCurrentArgument = $(f).closest(".launcher-argument[contenteditable=true]");
        if (launcherCurrentArgument.size() != 0)
        {
            launcherHead = $($(launcherCurrentArgument).parent().children()[0]);
            launcherCurrentArgumentRangeStart = window.getSelection().anchorOffset;
            return LauncherKeyPressedInCurrentArgument(event);
        }

        return true;
    })

    rootContainer = document.getElementById("all");
    message_callback = function() {
        Show();
    }
});
