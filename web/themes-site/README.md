# themes.nclabs.dev

Source files for the public, static SwitchU catalog page.

- `index.html` is structure only.
- `assets/styles.css` contains presentation.
- `assets/app.js` renders untrusted catalog JSON solely through DOM APIs; do not
  introduce `innerHTML`, inline event handlers, or third-party resources.
- `nginx/switchu-themes.conf` is the matching static-only server policy.

Deployment copies these files to `/srv/themes` in CT 123 and validates nginx
before it reloads. Catalog packages remain served from `/srv/themes/themes`;
they are validated separately by the ingestion worker and again by SwitchU.
