/**
 * Created by dennis on 31/10/16.
 */
(function() {

    var db = {
        loadData: function(filter) {
            return $.grep(this.rel, function(rel) {
                return (!filter.TOT_T || rel.TOT_T == filter.TOT_T)
                    && (!filter.NREC_T || rel.NREC_T == filter.NREC_T)
                    && (!filter.REC_T || rel.REC_T == filter.REC_T)
                    && (!filter.COPY_T || rel.COPY_T == filter.COPY_T)
                    && (!filter.TUPLES || rel.TUPLES == filter.TUPLES)
                    && (!filter.REL_NAME || rel.REL_NAME.indexOf(filter.REL_NAME) > -1)
                    && (!filter.ID || rel.ID.indexOf(filter.ID) > -1)
                    && (!filter.SRC || rel.SRC.indexOf(filter.SRC) > -1)
                    && (!filter.PERFOR || rel.PERFOR.indexOf(filter.PERFOR) > -1);
            });
        }
    };
    window.db = db;

    db.overview;
    $.ajax({
        async: false,
        url: "json/overview.json",
        success: function(result) {
            db.overview = result;
        }
    });

    db.properties;
    $.ajax({
        async: false,
        url: "json/properties.json",
        success: function(result) {
            db.properties = result;
        }
    });

    db.rel = [];
    $.ajax({
        async: false,
        url: "json/rel.json",
        success: function(result) {
            $.each(result, function(i, field) {
                db.rel[i] = {
                    TOT_T: field[0],
                    NREC_T: field[1],
                    REC_T: field[2],
                    COPY_T: field[3],
                    TUPLES: field[4],
                    REL_NAME: field[5],
                    ID: field[6],
                    SRC: field[7],
                    PERFOR: field[8]
                };
            });
        }
    });

    db.rul = [];
    $.ajax({
        async: false,
        url: "json/rul.json",
        success: function(result) {
            $.each(result, function(i, field) {
                db.rul[i] = {
                    TOT_T: field[0],
                    NREC_T: field[1],
                    REC_T: field[2],
                    COPY_T: field[3],
                    TUPLES: field[4],
                    RUL_NAME: field[5],
                    ID: field[6],
                    REL_NAME: field[7],
                    VER: field[8],
                    PERFOR: field[9],
                    SRC: field[10]
                };
            });
        }
    });
}());