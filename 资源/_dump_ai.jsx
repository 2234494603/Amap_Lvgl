#target illustrator

app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;

var docPath = "C:/Users/Administrator/Desktop/MCU_LVGL_GUI_Guider_submit/电动车智能显示屏修图-Q.ai";
var outPath = "C:/Users/Administrator/Desktop/MCU_LVGL_GUI_Guider_submit/ai_objects.tsv";

var doc = app.open(new File(docPath));

function enc(s) {
    if (s === undefined || s === null) s = "";
    return encodeURIComponent(String(s));
}

function boundsOf(item) {
    try {
        var b = item.visibleBounds; // [left, top, right, bottom]
        return [b[0], b[1], b[2] - b[0], b[1] - b[3]];
    } catch (e) {
        return [0, 0, 0, 0];
    }
}

function artboardOf(item) {
    try {
        var b = item.visibleBounds;
        var cx = (b[0] + b[2]) / 2;
        var cy = (b[1] + b[3]) / 2;
        for (var i = 0; i < doc.artboards.length; i++) {
            var r = doc.artboards[i].artboardRect;
            if (cx >= r[0] && cx <= r[2] && cy <= r[1] && cy >= r[3]) return i;
        }
    } catch (e) {}
    return -1;
}

var lines = [];
lines.push(["depth", "typename", "path", "name", "x", "y", "w", "h", "artboard", "hidden", "locked"].join("\t"));

function walk(items, depth, parentPath, maxDepth) {
    for (var i = 0; i < items.length; i++) {
        var it = items[i];
        var nm = "";
        try { nm = it.name; } catch (e) {}
        var type = "";
        try { type = it.typename; } catch (e) { type = "?"; }
        var b = boundsOf(it);
        var ab = artboardOf(it);
        var hidden = 0, locked = 0;
        try { hidden = it.hidden ? 1 : 0; } catch (e) {}
        try { locked = it.locked ? 1 : 0; } catch (e) {}
        var label = nm ? nm : type + "_" + i;
        var path = parentPath + "/" + label;
        lines.push([
            depth, type, enc(path), enc(nm),
            b[0].toFixed(2), b[1].toFixed(2), b[2].toFixed(2), b[3].toFixed(2),
            ab, hidden, locked
        ].join("\t"));

        if (depth < maxDepth) {
            var children = null;
            try { children = it.pageItems; } catch (e) {}
            if (children && children.length) {
                walk(children, depth + 1, path, maxDepth);
            }
        }
    }
}

for (var li = 0; li < doc.layers.length; li++) {
    var layer = doc.layers[li];
    lines.push([
        0, "Layer", enc("/" + layer.name), enc(layer.name),
        "", "", "", "", "", layer.visible ? 0 : 1, layer.locked ? 1 : 0
    ].join("\t"));
    try {
        walk(layer.pageItems, 1, "/" + layer.name, 3);
    } catch (e) {
        lines.push([1, "ERROR", enc(String(e)), "", "", "", "", "", "", "", ""].join("\t"));
    }
}

var f = new File(outPath);
f.encoding = "UTF-8";
f.open("w");
f.write(lines.join("\n"));
f.close();

var topCount = 0;
try { topCount = doc.layers[0].pageItems.length; } catch (e) {}
var summary = "layers=" + doc.layers.length +
    " artboards=" + doc.artboards.length +
    " topItems=" + topCount +
    " lines=" + lines.length;

doc.close(SaveOptions.DONOTSAVECHANGES);
summary;
