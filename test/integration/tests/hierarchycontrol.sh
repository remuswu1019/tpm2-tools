# SPDX-License-Identifier: BSD-3-Clause

source helpers.sh

cleanup() {

  if [ "$1" != "no-shut-down" ]; then
      shut_down
  fi
}
trap cleanup EXIT

start_up

cleanup "no-shut-down"

tpm2_clear

# Clear the handler
trap - ERR

# ERROR: phEnable may not be SET using this command
tpm2_hierarchycontrol -c p -e phEnable set

# EROOR: Only platform hierarchy handle can be specified for SET
tpm2_hierarchycontrol -c o -e shEnable set
tpm2_hierarchycontrol -c o -e ehEnable set
tpm2_hierarchycontrol -c o -e phEnable set
tpm2_hierarchycontrol -c o -e phEnableNV set
tpm2_hierarchycontrol -c e -e shEnable set
tpm2_hierarchycontrol -c e -e ehEnable set
tpm2_hierarchycontrol -c e -e phEnable set
tpm2_hierarchycontrol -c e -e phEnableNV set

# ERROR: Permanent handle lockout not supported by this command
tpm2_hierarchycontrol -c l -e shEnable set
tpm2_hierarchycontrol -c l -e ehEnable set
tpm2_hierarchycontrol -c l -e phEnable set
tpm2_hierarchycontrol -c l -e phEnableNV set
tpm2_hierarchycontrol -c l -e shEnable clear
tpm2_hierarchycontrol -c l -e ehEnable clear
tpm2_hierarchycontrol -c l -e phEnable clear
tpm2_hierarchycontrol -c l -e phEnableNV clear

# ERROR: Only platform and its authorization can be specified for CLEAR
tpm2_hierarchycontrol -c o -e ehEnable clear
tpm2_hierarchycontrol -c o -e phEnable clear
tpm2_hierarchycontrol -c o -e phEnableNV clear
tpm2_hierarchycontrol -c e -e shEnable clear
tpm2_hierarchycontrol -c e -e phEnable clear
tpm2_hierarchycontrol -c e -e phEnableNV clear

# Now set the trap handler for ERR
trap onerror ERR

# Storage hierarchy
tpm2_hierarchycontrol -c p -e shEnable set
tpm2_hierarchycontrol -c p -e shEnable clear
tpm2_hierarchycontrol -c p -e shEnable set
tpm2_hierarchycontrol -c o -e shEnable clear

# Endorsement hierarchy
tpm2_hierarchycontrol -c p -e ehEnable set
tpm2_hierarchycontrol -c p -e ehEnable clear
tpm2_hierarchycontrol -c p -e ehEnable set
tpm2_hierarchycontrol -c e -e ehEnable clear

# Platform NV
tpm2_hierarchycontrol -c p -e phEnableNV set
tpm2_hierarchycontrol -c p -e phEnableNV clear
tpm2_hierarchycontrol -c p -e phEnableNV set

# Platform hierarchy
tpm2_hierarchycontrol -c p -e phEnable clear

exit 0
