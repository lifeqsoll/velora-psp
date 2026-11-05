# Velora / OwnShell — native PSP-3008 shell (ARK)

Нативная оболочка **480×272** для PSP-3008 под **ARK/PRO**. Самостоятельный `EBOOT.PBP` (не React, не браузерный мок).

## Что есть в оболочке

- Вкладки **Игры / Музыка / Приложения / Файлы**, настройки через **Start** (иконка шестерёнки)
- Библиотека: обложки / сетка / список (сохраняется), избранное, недавние, поиск
- Тема: пресеты + круговая палитра стиком, акцент не красит весь текст
- **Файлы**: две локации L/R, копирование / перемещение / удаление / переименование с подтверждением; запись только на ms0/ef0
- **Музыка**: список локальных файлов; воспроизведение и внутриигровой звук — **отдельные этапы**
- **HOME**: только помеченный **предпросмотр** (не внутриигровой PRX)
- ISO-запуск честно требует CFW; savestate не подменяется обычными сейвами

## Сборка

```bash
export PATH="$HOME/pspdev/bin:$PATH"   # pspdev v20261101
make -C host_tests test
make -C shell
```

Скопировать `shell/EBOOT.PBP` → `ms0:/PSP/GAME/OWNSHELL/EBOOT.PBP`.

Готовый артефакт: `dist/OwnShell-M1-EBOOT.PBP`.

## Проверка

| Слой | Как |
|------|-----|
| **host** | `make -C host_tests test` |
| **emulator** | PPSSPP: UI, ввод, сохранение темы |
| **real PSP** | CFW, батарея/RTC, LoadExec ISO |

Autotest (опционально): файл `ms0:/PSP/SYSTEM/ownshell/autotest.cfg` циклически листает вкладки; Start выключает.

## Документация

- [docs/architecture.md](docs/architecture.md)
- [docs/compatibility-matrix.md](docs/compatibility-matrix.md)
- [docs/pinned-versions.md](docs/pinned-versions.md)
- [docs/hardware-checklist.md](docs/hardware-checklist.md)
- [THIRD_PARTY.md](THIRD_PARTY.md)

## Безопасность

- Нет записи во flash0/flash1 из оболочки
- Ничего не прошивается автоматически
- HOME PRX / Telegram / сеть — только после стабильной оболочки, отдельными этапами
