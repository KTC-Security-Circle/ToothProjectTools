sudo chown -R $(id -u):$(id -g) /workspace
sudo find /workspace -type d -exec chmod u+rwx {} + 
sudo mkdir -p /home/vscode/.local/opencv/4.10.0-static 
sudo chown -R $(id -u):$(id -g) /home/vscode/.local/opencv/4.10.0-static
GUIDE_SCRIPT="/workspace/.devcontainer/scripts/show-development-guide.sh"
BASHRC="$HOME/.bashrc"
MARKER="# ToothProjectTools development guide"

if ! grep -Fq "$MARKER" "$BASHRC"; then
  cat >> "$BASHRC" <<'EOF'

# ToothProjectTools development guide
if [[ $- == *i* ]] && [[ -x /workspace/.devcontainer/scripts/show-development-guide.sh ]]; then
  /workspace/.devcontainer/scripts/show-development-guide.sh
fi
EOF
fi