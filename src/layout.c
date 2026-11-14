/*
 * ReportLayout JSON çözümleyici
 *
 * KWin betiği her ekran için şu biçimde bir dizi gönderir:
 *   [{"output":"eDP-1","x":0,"y":0,"w":1920,"h":1080,
 *     "edge":"bottom","thickness":44,"scale":1.25,"fullscreen":false}]
 *
 * json-glib bağımlılığı eklememek için yalnız bu biçimi destekleyen küçük
 * bir okuyucu kullanılır. Bilinmeyen alanlar atlanır; bozuk belgede hata
 * döner ve `layout_apply` düzeni boşaltır.
 */
#include "layout.h"

typedef struct {
    const gchar *p;
} Cursor;

static void skip_ws(Cursor *c)
{
    while (g_ascii_isspace((guchar)*c->p)) {
        c->p++;
    }
}

static gboolean expect_char(Cursor *c, gchar ch)
{
    skip_ws(c);
    if (*c->p != ch) {
        return FALSE;
    }
    c->p++;
    return TRUE;
}

static gint hex_digit(gchar ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static gboolean parse_hex4(Cursor *c, guint *out)
{
    guint value = 0;

    for (gint i = 0; i < 4; i++) {
        gint digit = hex_digit(*c->p);

        if (digit < 0) {
            return FALSE;
        }
        value = value * 16 + (guint)digit;
        c->p++;
    }
    *out = value;
    return TRUE;
}

static gboolean parse_escape(Cursor *c, GString *out)
{
    gchar esc = *c->p;

    if (esc == '\0') {
        return FALSE;
    }
    c->p++;
    switch (esc) {
    case '"': g_string_append_c(out, '"'); return TRUE;
    case '\\': g_string_append_c(out, '\\'); return TRUE;
    case '/': g_string_append_c(out, '/'); return TRUE;
    case 'b': g_string_append_c(out, '\b'); return TRUE;
    case 'f': g_string_append_c(out, '\f'); return TRUE;
    case 'n': g_string_append_c(out, '\n'); return TRUE;
    case 'r': g_string_append_c(out, '\r'); return TRUE;
    case 't': g_string_append_c(out, '\t'); return TRUE;
    case 'u': break;
    default: return FALSE;
    }
    guint cp = 0;

    if (!parse_hex4(c, &cp)) {
        return FALSE;
    }
    if (cp >= 0xD800 && cp <= 0xDBFF && c->p[0] == '\\' && c->p[1] == 'u') {
        Cursor low = { c->p + 2 };
        guint lo = 0;

        if (parse_hex4(&low, &lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            c->p = low.p;
        }
    }
    gchar buf[6];
    gint len = g_unichar_to_utf8(cp, buf);

    g_string_append_len(out, buf, len);
    return TRUE;
}

static gboolean parse_string(Cursor *c, gchar **out)
{
    skip_ws(c);
    if (*c->p != '"') {
        return FALSE;
    }
    c->p++;
    GString *value = g_string_new(NULL);

    while (*c->p != '"') {
        guchar ch = (guchar)*c->p;

        if (ch == '\0' || ch < 0x20) {
            g_string_free(value, TRUE);
            return FALSE;
        }
        c->p++;
        if (ch == '\\' && !parse_escape(c, value)) {
            g_string_free(value, TRUE);
            return FALSE;
        }
        if (ch != '\\') {
            g_string_append_c(value, (gchar)ch);
        }
    }
    c->p++;
    *out = g_string_free(value, FALSE);
    return TRUE;
}

static gboolean parse_number(Cursor *c, gdouble *out)
{
    skip_ws(c);
    const gchar *start = c->p;

    if (*c->p == '-') {
        c->p++;
    }
    if (!g_ascii_isdigit(*c->p)) {
        return FALSE;
    }
    while (g_ascii_isdigit(*c->p)) {
        c->p++;
    }
    if (*c->p == '.') {
        c->p++;
        if (!g_ascii_isdigit(*c->p)) {
            return FALSE;
        }
        while (g_ascii_isdigit(*c->p)) {
            c->p++;
        }
    }
    if (*c->p == 'e' || *c->p == 'E') {
        c->p++;
        if (*c->p == '+' || *c->p == '-') {
            c->p++;
        }
        if (!g_ascii_isdigit(*c->p)) {
            return FALSE;
        }
        while (g_ascii_isdigit(*c->p)) {
            c->p++;
        }
    }
    gchar *end = NULL;
    gdouble value = g_ascii_strtod(start, &end);

    if (end != c->p) {
        return FALSE;
    }
    *out = value;
    return TRUE;
}

static gboolean parse_bool(Cursor *c, gboolean *out)
{
    skip_ws(c);
    if (g_str_has_prefix(c->p, "true")) {
        c->p += 4;
        *out = TRUE;
        return TRUE;
    }
    if (g_str_has_prefix(c->p, "false")) {
        c->p += 5;
        *out = FALSE;
        return TRUE;
    }
    return FALSE;
}

static gboolean skip_value(Cursor *c, guint depth);

static gboolean skip_container(Cursor *c, gchar open, gchar close, guint depth)
{
    if (!expect_char(c, open)) {
        return FALSE;
    }
    skip_ws(c);
    if (*c->p == close) {
        c->p++;
        return TRUE;
    }
    for (;;) {
        if (open == '{') {
            gchar *key = NULL;

            if (!parse_string(c, &key) || !expect_char(c, ':')) {
                g_free(key);
                return FALSE;
            }
            g_free(key);
        }
        if (!skip_value(c, depth + 1)) {
            return FALSE;
        }
        skip_ws(c);
        if (*c->p == ',') {
            c->p++;
            continue;
        }
        if (*c->p == close) {
            c->p++;
            return TRUE;
        }
        return FALSE;
    }
}

static gboolean skip_value(Cursor *c, guint depth)
{
    if (depth > 32) {
        return FALSE;
    }
    skip_ws(c);
    switch (*c->p) {
    case '"': {
        gchar *value = NULL;
        gboolean ok = parse_string(c, &value);

        g_free(value);
        return ok;
    }
    case '{':
        return skip_container(c, '{', '}', depth);
    case '[':
        return skip_container(c, '[', ']', depth);
    case 't':
    case 'f': {
        gboolean value = FALSE;
        return parse_bool(c, &value);
    }
    case 'n':
        if (g_str_has_prefix(c->p, "null")) {
            c->p += 4;
            return TRUE;
        }
        return FALSE;
    default: {
        gdouble value = 0;

        return parse_number(c, &value);
    }
    }
}

static gboolean valid_edge(const gchar *edge)
{
    return g_strcmp0(edge, "bottom") == 0 || g_strcmp0(edge, "top") == 0
        || g_strcmp0(edge, "left") == 0 || g_strcmp0(edge, "right") == 0
        || g_strcmp0(edge, "none") == 0;
}

static gboolean set_int_field(Cursor *c, gint *out)
{
    gdouble value = 0;

    if (!parse_number(c, &value)) {
        return FALSE;
    }
    if (value > G_MAXINT) {
        value = G_MAXINT;
    } else if (value < G_MININT) {
        value = G_MININT;
    }
    *out = (gint)value;
    return TRUE;
}

static gboolean entry_field(Cursor *c, LayoutEntry *entry, const gchar *key)
{
    if (g_strcmp0(key, "output") == 0) {
        g_free(entry->output);
        return parse_string(c, &entry->output);
    }
    if (g_strcmp0(key, "edge") == 0) {
        gchar *value = NULL;

        if (!parse_string(c, &value)) {
            return FALSE;
        }
        g_free(entry->edge);
        entry->edge = valid_edge(value) ? value : g_strdup("none");
        if (entry->edge != value) {
            g_free(value);
        }
        return TRUE;
    }
    if (g_strcmp0(key, "x") == 0) {
        return set_int_field(c, &entry->x);
    }
    if (g_strcmp0(key, "y") == 0) {
        return set_int_field(c, &entry->y);
    }
    if (g_strcmp0(key, "w") == 0) {
        return set_int_field(c, &entry->w);
    }
    if (g_strcmp0(key, "h") == 0) {
        return set_int_field(c, &entry->h);
    }
    if (g_strcmp0(key, "thickness") == 0) {
        return set_int_field(c, &entry->thickness);
    }
    if (g_strcmp0(key, "scale") == 0) {
        gdouble value = 1;

        if (!parse_number(c, &value)) {
            return FALSE;
        }
        entry->scale = value > 0 ? value : 1;
        return TRUE;
    }
    if (g_strcmp0(key, "fullscreen") == 0) {
        return parse_bool(c, &entry->fullscreen);
    }
    return skip_value(c, 0);
}

static void entry_clear(LayoutEntry *entry)
{
    g_free(entry->output);
    g_free(entry->edge);
}

static gboolean entry_parse(Cursor *c, LayoutEntry *entry)
{
    entry->edge = g_strdup("none");
    entry->scale = 1;
    if (!expect_char(c, '{')) {
        return FALSE;
    }
    skip_ws(c);
    if (*c->p == '}') {
        c->p++;
        return TRUE;
    }
    for (;;) {
        gchar *key = NULL;
        gboolean ok;

        if (!parse_string(c, &key) || !expect_char(c, ':')) {
            g_free(key);
            return FALSE;
        }
        ok = entry_field(c, entry, key);
        g_free(key);
        if (!ok) {
            return FALSE;
        }
        skip_ws(c);
        if (*c->p == ',') {
            c->p++;
            continue;
        }
        if (*c->p == '}') {
            c->p++;
            return TRUE;
        }
        return FALSE;
    }
}

gboolean layout_parse(const gchar *json, LayoutEntry **entries, gsize *len,
                      GError **error)
{
    g_return_val_if_fail(entries != NULL, FALSE);
    g_return_val_if_fail(len != NULL, FALSE);

    *entries = NULL;
    *len = 0;
    Cursor c = { json };
    GArray *array = g_array_new(FALSE, FALSE, sizeof(LayoutEntry));
    gboolean ok = FALSE;

    if (json == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "düzen JSON'u boş");
        g_array_free(array, TRUE);
        return FALSE;
    }
    skip_ws(&c);
    if (*c.p != '[') {
        goto done;
    }
    c.p++;
    skip_ws(&c);
    if (*c.p == ']') {
        c.p++;
    } else {
        for (;;) {
            LayoutEntry zero = { 0 };

            g_array_append_val(array, zero);
            LayoutEntry *entry =
                &g_array_index(array, LayoutEntry, array->len - 1);
            if (!entry_parse(&c, entry)) {
                goto done;
            }
            skip_ws(&c);
            if (*c.p == ',') {
                c.p++;
                continue;
            }
            if (*c.p != ']') {
                goto done;
            }
            c.p++;
            break;
        }
    }
    skip_ws(&c);
    ok = *c.p == '\0';

done:
    if (!ok) {
        for (guint i = 0; i < array->len; i++) {
            entry_clear(&g_array_index(array, LayoutEntry, i));
        }
        g_array_free(array, TRUE);
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                    "geçersiz düzen JSON'u (konum %ld)",
                    (long)(c.p - json));
        return FALSE;
    }
    *len = array->len;
    *entries = (LayoutEntry *)g_array_free(array, FALSE);
    return TRUE;
}

void layout_entries_free(LayoutEntry *entries, gsize len)
{
    if (entries == NULL) {
        return;
    }
    for (gsize i = 0; i < len; i++) {
        entry_clear(&entries[i]);
    }
    g_free(entries);
}

const LayoutEntry *layout_find(const LayoutEntry *entries, gsize len,
                               const gchar *output)
{
    if (entries == NULL || output == NULL) {
        return NULL;
    }
    for (gsize i = 0; i < len; i++) {
        if (g_strcmp0(entries[i].output, output) == 0) {
            return &entries[i];
        }
    }
    return NULL;
}

gboolean layout_apply(App *app, const gchar *json)
{
    LayoutEntry *entries = NULL;
    gsize len = 0;
    GError *error = NULL;

    app_clear_layout(app);
    if (!layout_parse(json, &entries, &len, &error)) {
        app_warn(app, "düzen raporu çözümlenemedi: %s",
                 error != NULL ? error->message : "bilinmeyen hata");
        g_clear_error(&error);
        return FALSE;
    }
    app->layout = entries;
    app->layout_len = len;
    return TRUE;
}
