/* Catalog data is untrusted. Build nodes with DOM APIs; never parse it as HTML. */
(() => {
  'use strict';
  const output = document.querySelector('#out');
  const readout = document.querySelector('#readout');
  const motionToggle = document.querySelector('#motion-toggle');
  // Previews are content in this catalog, not decoration. Browser/OS reduced
  // motion previously turned every theme into a still without a visible way
  // to restore it. Keep the choice explicit and remember it per visitor.
  let motionEnabled = true;
  try { motionEnabled = localStorage.getItem('switchu-theme-motion') !== 'off'; } catch { /* storage optional */ }
  let themes = [];

  const element = (name, className, text) => {
    const node = document.createElement(name);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = String(text);
    return node;
  };
  const integer = value => Number.isFinite(Number(value)) ? Math.max(0, Math.floor(Number(value))) : 0;
  const megabytes = value => { const bytes = integer(value); return bytes >= 1048576 ? `${(bytes / 1048576).toFixed(1)} MB` : `${Math.round(bytes / 1024)} KB`; };
  const appendText = (parent, strong, text) => { const span = element('span'); if (strong !== undefined) span.append(element('b', '', strong)); span.append(` ${text}`); parent.append(span); };

  // Only same-origin, relative paths are allowed from JSON. URL() also rejects
  // traversal after normalisation, protocol-relative URLs, and encoded surprises.
  const safePath = value => {
    if (typeof value !== 'string' || value.length === 0 || value.length > 240 || /[\\\0]/.test(value)) return null;
    try {
      const base = new URL(`${location.origin}${location.pathname.replace(/[^/]*$/, '')}`);
      const url = new URL(value, base);
      if (url.origin !== location.origin || !url.pathname.startsWith(base.pathname) || url.search || url.hash) return null;
      return `${url.pathname}${url.search}`;
    } catch { return null; }
  };
  const asset = (tag, path, options = {}) => {
    const safe = safePath(path);
    if (!safe) return null;
    const node = element(tag);
    node.src = safe;
    Object.assign(node, options);
    return node;
  };
  const flag = (text, state = '') => element('span', `flag ${state}`.trim(), text);
  const clear = className => { output.replaceChildren(); output.className = className; };

  function renderReadout() {
    readout.replaceChildren();
    const totals = themes.reduce((value, theme) => ({ frames: value.frames + integer(theme.frameCount), bytes: value.bytes + integer(theme.bytes), animated: value.animated + Boolean(theme.animated) }), { frames: 0, bytes: 0, animated: 0 });
    appendText(readout, themes.length, 'temas'); appendText(readout, totals.animated, 'animados'); appendText(readout, totals.frames, 'quadros'); appendText(readout, megabytes(totals.bytes), ''); appendText(readout, 'src', `${location.host}/index.json`);
  }
  function renderMotionToggle() {
    motionToggle.setAttribute('aria-pressed', String(motionEnabled));
    motionToggle.textContent = motionEnabled ? 'Animações: ligadas' : 'Animações: desligadas';
  }
  function mediaFor(theme, stage = false) {
    const video = motionEnabled && safePath(theme.previewVideo);
    if (video) {
      const media = asset('video', video, { muted: true, loop: true, playsInline: true, preload: stage ? 'metadata' : 'none' });
      const poster = safePath(theme.previewPoster); if (poster) media.poster = poster;
      if (stage) { media.autoplay = true; } else { media.dataset.src = video; media.removeAttribute('src'); }
      return media;
    }
    return asset('img', reducedMotion ? (theme.previewPoster || theme.cover) : (theme.preview || theme.cover), { loading: stage ? 'eager' : 'lazy', alt: '' });
  }
  function observeVideos() {
    const videos = output.querySelectorAll('video[data-src]');
    if (!videos.length) return;
    const activate = video => { if (!video.src) { video.src = video.dataset.src; video.play().catch(() => {}); } };
    if (!('IntersectionObserver' in window)) { [...videos].slice(0, 6).forEach(activate); return; }
    const observer = new IntersectionObserver(entries => entries.forEach(({ target, isIntersecting }) => isIntersecting ? activate(target) : target.pause()), { rootMargin: '200px' });
    videos.forEach(video => observer.observe(video));
  }
  function renderGrid() {
    if (!themes.length) { clear('msg'); output.textContent = 'Catálogo vazio'; return; }
    clear('grid');
    themes.forEach((theme, index) => {
      const tile = element('button', 'tile'); tile.type = 'button'; tile.style.animationDelay = `${index * 45}ms`;
      tile.addEventListener('click', () => { location.hash = encodeURIComponent(String(theme.id || '')); });
      const frame = element('div', 'frame'); frame.append(element('span', 'chan', String(index + 1).padStart(2, '0'))); const media = mediaFor(theme); if (media) frame.append(media);
      const meta = element('div', 'meta'); meta.append(element('span', 'nm', theme.name || 'Tema sem nome'), element('span', 'sub', `${theme.author || 'Autor desconhecido'} · v${theme.version || '-'} · ${megabytes(theme.bytes)}`));
      const flags = element('span', 'flags'); flags.append(theme.animated ? flag(`${integer(theme.frameCount)} quadros / ${theme.fps || '-'} fps`, 'on') : flag('estático')); if (theme.music) flags.append(flag('trilha', 'on')); flags.append(theme.license ? flag(theme.license) : flag('sem licença', 'warn')); meta.append(flags); tile.append(frame, meta); output.append(tile);
    });
    observeVideos();
  }
  const row = (label, value) => { const line = element('div'); line.append(element('dt', '', label), element('i', 'dots'), element('dd', '', value)); return line; };
  function renderDetail(theme, manifest = {}) {
    clear(''); const back = element('button', 'back', '← catálogo'); back.type = 'button'; back.addEventListener('click', () => { location.hash = ''; }); output.append(back);
    const detail = element('div', 'detail'), visual = element('div'), stage = element('div', 'stage'); const media = mediaFor(theme, true); if (media) stage.append(media); visual.append(stage);
    const screenshots = Array.isArray(manifest.preview?.screenshots) ? manifest.preview.screenshots.map(name => safePath(`${String(theme.path || '').replace(/^\/+|\/+$/g, '')}/${name}`)).filter(Boolean) : [];
    if (screenshots.length) { const strip = element('div', 'strip'); screenshots.forEach(path => { const image = asset('img', path, { loading: 'lazy', alt: '' }); image.addEventListener('click', () => stage.replaceChildren(asset('img', path, { alt: '' }))); strip.append(image); }); visual.append(strip); }
    const info = element('div'); info.append(element('h2', 'title', theme.name || 'Tema sem nome'), element('p', 'sub', theme.author || 'Autor desconhecido'));
    const spec = element('dl', 'spec'); spec.append(row('id', theme.id || '-'), row('versão', theme.version || '-'), row('modo', manifest.theme?.mode || '-'), row('fundo', theme.animated ? `${integer(theme.frameCount)} quadros @ ${theme.fps || '-'} fps` : 'imagem estática'), row('trilha', theme.music ? `${integer(theme.music)} faixa(s)` : 'nenhuma'), row('tamanho', megabytes(theme.bytes)), row('licença', theme.license || 'não declarada')); info.append(spec);
    if (!theme.license) info.append(element('p', 'notice', 'Sem licença declarada. O tema não deve ser promovido sem licença e origem da arte.'));
    const manifestPath = safePath(theme.manifest); if (manifestPath) { const endpoint = element('div', 'endpoint'); endpoint.append(element('b', '', 'manifesto que o console baixa'), element('span', '', `${location.origin}${manifestPath}`)); info.append(endpoint); }
    detail.append(visual, info); output.append(detail);
  }
  async function route() {
    let id = ''; try { id = decodeURIComponent(location.hash.slice(1)); } catch { /* ignored */ }
    window.scrollTo(0, 0); const theme = themes.find(item => String(item.id) === id); if (!theme) return renderGrid();
    clear('msg'); output.textContent = 'Lendo manifesto…'; const manifestPath = safePath(theme.manifest);
    try { const response = await fetch(manifestPath, { cache: 'no-cache', credentials: 'same-origin' }); renderDetail(theme, response.ok ? await response.json() : {}); } catch { renderDetail(theme); }
  }
  document.addEventListener('keydown', event => { if (event.key === 'Escape') location.hash = ''; }); window.addEventListener('hashchange', route);
  motionToggle.addEventListener('click', () => {
    motionEnabled = !motionEnabled;
    try { localStorage.setItem('switchu-theme-motion', motionEnabled ? 'on' : 'off'); } catch { /* storage optional */ }
    renderMotionToggle(); route();
  });
  renderMotionToggle();
  fetch('review.json', { cache: 'no-cache', credentials: 'same-origin' }).then(response => response.ok ? response.json() : Promise.reject(new Error(`HTTP ${response.status}`))).then(data => { themes = Array.isArray(data?.themes) ? data.themes : []; renderReadout(); route(); }).catch(() => { clear('msg bad'); output.textContent = 'Não consegui ler o catálogo.'; });
})();
