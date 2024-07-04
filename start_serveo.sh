#!/bin/bash

# Dirección MAC del ESP32
ESP32_MAC="3c:71:bf:f8:ab:a0"  # Reemplaza con la dirección MAC de tu ESP32

# Obtener el nombre de usuario original que invocó sudo
ORIGINAL_USER=${SUDO_USER:-$(whoami)}

# Ruta a la clave SSH en el directorio home del usuario original
SSH_KEY_PATH="/home/$ORIGINAL_USER/.ssh/serveo_key"

# Verificar si la clave SSH existe y es accesible
if [ ! -f "$SSH_KEY_PATH" ]; then
    echo "La clave SSH no se encuentra en $SSH_KEY_PATH"
    exit 1
fi

# Asegurar que la clave SSH tenga los permisos correctos
chmod 600 "$SSH_KEY_PATH"

# Función para encontrar la IP del ESP32
find_esp32_ip() {
    echo "Ejecutando arp-scan..."
    sudo arp-scan --localnet > /tmp/arp-scan-output.txt
    IP_LINE=$(grep "$ESP32_MAC" /tmp/arp-scan-output.txt)
    if [ -z "$IP_LINE" ]; then
        echo "No se pudo encontrar la IP del ESP32"
        exit 1
    fi
    ESP32_IP=$(echo $IP_LINE | awk '{print $1}')
    echo "IP encontrada: $ESP32_IP"
    echo $ESP32_IP
}

# Buscar la IP del ESP32
ESP32_IP=$(find_esp32_ip | tail -n 1)

# Verificar si se encontró la IP
if [ -z "$ESP32_IP" ]; then
    echo "No se pudo encontrar la IP del ESP32. Asegúrate de que está conectado a la red."
    exit 1
else
    echo "La IP del ESP32 es $ESP32_IP"
fi

# Iniciar Serveo
echo "Iniciando Serveo con IP $ESP32_IP"
ssh -i "$SSH_KEY_PATH" -o ServerAliveInterval=60 -R centinel:80:$ESP32_IP:80 serveo.net

