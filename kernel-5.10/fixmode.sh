#!/bin/bash

git diff --summary | grep '^[[:space:]]*mode change' | while IFS= read -r line; do
  # Count how many fields are in the line
  field_count=$(echo "$line" | awk '{print NF}')
  
  # If fewer than 6 fields, assume no file name is present and skip this line.
  if [ "$field_count" -lt 6 ]; then
    echo -e "Skipping line (no file provided): $line"
    continue
  fi
  
  # Extract the file name.
  # Here, we join fields 6 to NF to account for file names containing spaces.
  file=$(echo "$line" | awk '{for(i=6;i<=NF;i++) printf $i " "; print ""}' | sed 's/[[:space:]]*$//')
  
  # Extract the full original mode from field 3.
  old_mode_full=$(echo "$line" | awk '{print $3}')
  
  # Extract only the last three characters (e.g. "755" or "644").
  old_mode=${old_mode_full: -3}
  
  echo "Restoring mode $old_mode on file: $file"
  chmod "$old_mode" "$file"
done
