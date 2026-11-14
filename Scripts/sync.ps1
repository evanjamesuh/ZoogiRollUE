# Expands the source zip delivered from the build box into the project (does not touch Content/)
param([string]$Zip = "C:\Users\skybi\zoogi_xfer\proj.zip")
Expand-Archive -Force -Path $Zip -DestinationPath C:\dev\ZoogiRollUnreal
