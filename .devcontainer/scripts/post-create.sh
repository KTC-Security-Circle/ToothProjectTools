sudo chown -R $(id -u):$(id -g) /workspace
sudo find /workspace -type d -exec chmod u+rwx {} + 
sudo mkdir -p /home/vscode/.local/opencv/4.10.0-static 
sudo chown -R $(id -u):$(id -g) /home/vscode/.local/opencv/4.10.0-static