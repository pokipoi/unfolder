# PowerShell wrapper to collect selected folders and pass them to unfolder.exe
param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Folders
)

# Get the directory where this script is located
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath = Join-Path $scriptDir "unfolder.exe"

# Check if we have any folders
if ($Folders.Count -eq 0) {
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show(
        "No folders selected. Please select one or more folders and try again.",
        "Error",
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Error
    )
    exit 1
}

# Call unfolder.exe with all selected folders
& $exePath @Folders
