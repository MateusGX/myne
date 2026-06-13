# Translators

Myne currently ships with **English** and **Portuguese** (see [i18n.md](./i18n.md) for the languages
list and file format).

## Contributing a translation

1. Copy `lib/I18n/translations/english.yaml` to a new file for your language (e.g.
   `lib/I18n/translations/italian.yaml`), set `_language_name`, `_language_code`, and `_order`, and
   translate the `STR_*` values.
2. Run `python3 scripts/gen_i18n.py lib/I18n/translations lib/I18n/` to regenerate the generated I18n
   files and check for issues.
3. Keep translations concise — the e-ink UI has limited space for text.
4. Open a PR with your new YAML file (the generated `I18nKeys.h`/`I18nStrings.{h,cpp}` are gitignored
   and don't need to be committed).

Updating an existing translation works the same way — edit the YAML file and regenerate.

If you're only fixing a few strings rather than adding a whole new language, a small PR touching just
the relevant `STR_*` keys is just as welcome.
