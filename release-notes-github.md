# SwitchU 2.4.3

## English

Fix for manual date and time modification, non-blocking public SNTP pool time synchronization, and multi-thread toast safety.

### Date and time management

- Fixed manual date/time modification in SwitchU daemon: resolved Horizon OS permission denial (`0x274` / `Time::PermissionDenied`) by directly configuring `NetworkSystemClock` (`time:s` cmd 1) and `LocalSystemClock` (`time:a` cmd 4) instead of relying on stock automatic correction toggles.
- Added public SNTP time synchronization client supporting `pool.ntp.org` pools (`0.pool.ntp.org` through `3.pool.ntp.org`) and fallbacks (`time.google.com`, `time.cloudflare.com`). Enables reliable network time synchronization on consoles where stock Nintendo telemetry is blocked by 90DNS or Atmosphère hosts.
- Added a dedicated "Synchronize Clock Now" action button in the System settings tab.
- Toggling "Synchronize Clock via Internet" now triggers immediate background SNTP query with on-screen toast feedback.
- Implemented background worker thread via libnx native Horizon `Thread` API pinned to Core 2, avoiding runtime aborts and preserving 60 FPS UI performance.
- Hardened toast message presentation (`TabbedOverlayScreen`) with thread-safe mutual exclusion for background worker notifications.

---

## Português

Correção na alteração manual de data e hora, sincronização de horário via pools SNTP públicos e segurança de threads para notificações toast.

### Gerenciamento de data e hora

- Corrigida a alteração manual de data e hora no daemon do SwitchU: solucionado o erro de permissão do Horizon OS (`0x274` / `Time::PermissionDenied`) através do acesso direto via IPC ao `NetworkSystemClock` (`time:s` cmd 1) e `LocalSystemClock` (`time:a` cmd 4).
- Adicionado cliente SNTP para sincronização de horário através dos pools públicos do `pool.ntp.org` (`0.pool.ntp.org` a `3.pool.ntp.org`) e servidores de contingência (`time.google.com`, `time.cloudflare.com`). Permite sincronizar a hora pela rede mesmo em consoles com bloqueio de telemetria da Nintendo via 90DNS ou hosts do Atmosphère.
- Adicionado botão de ação "Sincronizar relógio agora" na aba de Sistema das configurações.
- Ativar a opção "Sincronizar relógio pela Internet" agora dispara sincronização imediata em segundo plano com feedback em toast.
- Implementada execução em segundo plano utilizando threads nativas do Horizon OS (`Thread` da libnx) fixadas no Core 2, eliminando falhas de runtime e mantendo a interface fluida a 60 FPS.
- Protegida a exibição de notificações toast (`TabbedOverlayScreen`) com exclusão mútua (`mutex`) para despacho seguro a partir de threads secundárias.
