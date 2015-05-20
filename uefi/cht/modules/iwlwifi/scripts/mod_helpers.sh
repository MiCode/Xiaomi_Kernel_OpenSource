function mod_filename()
{
	which modinfo > /dev/null 2>&1
	if [[ $? -eq 0 ]]; then
		MOD_QUERY="modinfo -F filename"
	else
		MOD_QUERY="modprobe -l"
	fi
	mod_path="$($MOD_QUERY $1 | tail -1)"
	echo $(basename "$mod_path")
}
