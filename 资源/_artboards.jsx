#target illustrator

app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;

var docPath = "C:/Users/Administrator/Desktop/MCU_LVGL_GUI_Guider_submit/电动车智能显示屏修图-Q.ai";
var outPath = "C:/Users/Administrator/Desktop/MCU_LVGL_GUI_Guider_submit/ai_artboards.tsv";

var doc = app.open(new File(docPath));
var lines = [];
lines.push(["index", "name", "left", "top", "right", "bottom", "width", "height"].join("\t"));

for (var i = 0; i < doc.artboards.length; i++) {
    var ab = doc.artboards[i];
    var r = ab.artboardRect;
    lines.push([
        i, encodeURIComponent(ab.name),
        r[0].toFixed(3), r[1].toFixed(3), r[2].toFixed(3), r[3].toFixed(3),
        (r[2] - r[0]).toFixed(3), (r[1] - r[3]).toFixed(3)
    ].join("\t"));
}

var f = new File(outPath);
f.encoding = "UTF-8";
f.open("w");
f.write(lines.join("\n"));
f.close();

var summary = "artboards=" + doc.artboards.length;
doc.close(SaveOptions.DONOTSAVECHANGES);
summary;
