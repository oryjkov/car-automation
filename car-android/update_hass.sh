set -e
docker pull ghcr.io/home-assistant/home-assistant:stable 
docker stop homeassistant || true
docker rm homeassistant || true
docker run -d --name homeassistant --restart=unless-stopped -v /storage/self/primary/Download/config:/config --network=host ghcr.io/home-assistant/home-assistant:stable
