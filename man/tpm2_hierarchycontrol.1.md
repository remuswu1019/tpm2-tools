% tpm2_hierarchycontrol(1) tpm2-tools | General Commands Manual
%
% July 2019

# NAME

**tpm2_hierarchycontrol**(1) - Enable and disable use of a hierarchy and its
associated NV storage.

# SYNOPSIS

**tpm2_hierarchycontrol** [*OPTIONS*] _OPERATION_

# DESCRIPTION

**tpm2_hierarchycontrol**(1) - Allows user change phEnable, phEnableNV,
shEnable and ehEnable when the proper authorization is provided. Authorization
 should be one out of owner hierarchy auth, endorsement hierarchy auth and
 platform hierarchy auth. **As an argument the tool takes the \_OPERATION\_ as
 string clear|set to clear or set the TPMA_STARTUP_CLEAR bits.**
Note: If password option is missing, assume NULL.

# OPTIONS

  * **-C**, **\--hierarchy**=_AUTH\_HIERARCHY_:

    Specifies the handle used to authorize. Defaults to **p**, **TPM_RH_PLATFORM**,
    when no value has been specified.
    Supported options are:
      * **o** for **TPM_RH_OWNER**
      * **p** for **TPM_RH_PLATFORM**
      * **`<num>`** where a raw number can be used.

  * **-P**, **\--auth-hierarchy**=_AUTH\_HIERARCHY\_VALUE_:

    Specifies the authorization value for the hierarchy. Authorization values
    should follow the "authorization formatting standards", see section
    "Authorization Formatting".

  * **-e**, **\--enable**=_TPMA\_STARTUP\_CLEAR\_BITS:

    The TPMA_STARTUP_CLEAR attribute value.

[common options](common/options.md)

[common tcti options](common/tcti.md)

[authorization formatting](common/authorizations.md)

# EXAMPLES

## Set phEnableNV with platform hierarchy and its authorization
```
tpm2_hierarchycontrol -C p -e phEnableNV set -P pass
```

## clear phEnableNV with platform hierarchy
```
tpm2_hierarchycontrol -C p -e phEnableNV clear
```

## Set shEnable with platform hierarchy
```
tpm2_hierarchycontrol -C p -e shEnable set
```

## Set shEnable with owner hierarchy
```
tpm2_hierarchycontrol -C o -e shEnable set
```

## Check current TPMA_STARTUP_CLEAR Bits
```
tpm2_getcap -c properties-variable
```

[returns](common/returns.md)

[footer](common/footer.md)
