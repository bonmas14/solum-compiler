$pwd = Get-Location 
$modules = "modules"
$value = "$pwd\$modules\"
$env   = Test-Path -Path Env:\SOLUM_MODULES

if ( -not (Test-Path -Path "$value")) {
    Write-Host "Couldn't find modules folder. '$value'"
}

[System.Environment]::SetEnvironmentVariable("SOLUM_MODULES", $value, [System.EnvironmentVariableTarget]::Machine)

