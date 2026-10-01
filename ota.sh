#!/usr/bin/env bash
# OTA-заливка Speedo: проверяет зависимости PlatformIO и шьёт по WiFi.
#
# Перед запуском:
#   1. На устройстве: меню (долгое SET) -> «Обновление (WiFi)»
#   2. Подключить ноутбук к сети Speedo (пароль в src/secrets.h)
#
# Если зависимостей не хватает и есть интернет — докачает их сам
# (но в сети Speedo интернета нет, тогда сначала прогони
#  `pio run -e esp32-c3-ota` на домашнем WiFi).

set -e
cd "$(dirname "$0")"

PIO=".venv-pio/bin/pio"
[ -x "$PIO" ] || PIO="$(command -v pio || true)"
if [ -z "$PIO" ]; then
  echo "Ошибка: PlatformIO не найден (ни .venv-pio/bin/pio, ни pio в PATH)" >&2
  exit 1
fi

# 1. Зависимости: openocd и прочие пакеты платы должны быть в кэше —
#    в сети Speedo интернета нет, скачать их не выйдет.
NEED=(tool-openocd-esp32 tool-esptoolpy toolchain-riscv32-esp)
PKGDIR="$HOME/.platformio/packages"
MISSING=()
for p in "${NEED[@]}"; do
  [ -d "$PKGDIR/$p" ] || MISSING+=("$p")
done

if [ ${#MISSING[@]} -gt 0 ]; then
  echo "Не хватает пакетов: ${MISSING[*]}"
  echo "Пробую собрать проект (нужен интернет — домашний WiFi, не Speedo)..."
  "$PIO" run -e esp32-c3-ota
fi

# 2. Сборка (инкрементальная — быстрая, если уже собрано)
"$PIO" run -e esp32-c3-ota

# 3. Проверяем, что устройство в режиме OTA и доступно
if ! ping -c1 -W3 192.168.4.1 >/dev/null 2>&1; then
  cat >&2 <<'EOF'
Устройство не отвечает (192.168.4.1 недоступен).
  1. Меню (долгое SET) -> «Обновление (WiFi)»
  2. Подключись к сети Speedo
  3. Запусти скрипт ещё раз
EOF
  exit 1
fi

# 4. Пароль OTA: из OTA_PASSWORD, иначе из src/secrets.h
if [ -z "$OTA_PASSWORD" ]; then
  OTA_PASSWORD="$(sed -n 's/^#define OTA_PASS *"\(.*\)"/\1/p' src/secrets.h)"
fi
if [ -z "$OTA_PASSWORD" ]; then
  echo "Ошибка: не найден пароль OTA (ни OTA_PASSWORD, ни OTA_PASS в secrets.h)" >&2
  exit 1
fi

# 5. Заливка
export OTA_PASSWORD
exec "$PIO" run -e esp32-c3-ota -t upload
