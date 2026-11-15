/*
 * Çeviri altyapısı
 *
 * Kaynak dil Türkçe; msgid'ler Türkçe metinlerdir. `po/en.po` İngilizce
 * çevirileri sağlar. Arayüz dili ayarı `auto` değilse LANGUAGE ortam
 * değişkeni gettext ilk kullanılmadan önce ayarlanır.
 */
#pragma once

#include <glib.h>

#ifndef GETTEXT_PACKAGE
#define GETTEXT_PACKAGE "comebackwindow"
#endif

#include <glib/gi18n.h>

G_BEGIN_DECLS

/* Dil ayarını uygular ve çeviri kataloğunu bağlar. */
void i18n_init(const gchar *language);

G_END_DECLS
