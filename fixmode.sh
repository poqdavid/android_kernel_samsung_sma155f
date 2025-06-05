git diff --summary | grep '^[[:space:]]*mode change' | while read -r line; do

  # Extract the file path and the original mode.
  file=$(echo "$line" | awk '{print $NF}')
  old_mode_full=$(echo "$line" | awk '{print $3}')
  
  # Remove the first three characters to get 644 (or 755, etc.)
  old_mode=${old_mode_full: -3}
  
  # Apply the original mode to the file.
  chmod "$old_mode" "$file"
done
