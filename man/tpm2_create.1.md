% tpm2_create(1) tpm2-tools | General Commands Manual

# NAME

**tpm2_create**(1) - Create a child object.

# SYNOPSIS

**tpm2_create** [*OPTIONS*]

# DESCRIPTION

**tpm2_create**(1) - Create a child object. The object can either be a key or
a sealing object. A sealing object allows to seal user data to the TPM, with a
maximum size of 256 bytes. Additionally it will load the created object if the
**-o** is specified.

# OPTIONS

These options for creating the TPM entity:

  * **-C**, **\--parent-context**=_PARENT\_CONTEXT\_OBJECT_:

    Context object for the created object's parent. Either a file or a handle
    number. See section "Context Object Format".

  * **-P**, **\--parent-auth**=_PARENT\_KEY\_AUTH_:

    The authorization value for using the parent key, optional.
    Authorization values should follow the "authorization formatting standards",
    see section "Authorization Formatting".

  * **-p**, **\--key-auth**=_KEY\_AUTH_:

    The authorization value for the key, optional.
    Follows the authorization formatting of the
    "password for parent key" option: **-P**.

  * **-g**, **\--hash-algorithm**=_ALGORITHM_:

    The hash algorithm for generating the objects name. This is optional
    and defaults to sha256 when not specified. Algorithms should follow the
    "formatting standards", see section "Algorithm Specifiers".
    Also, see section "Supported Hash Algorithms" for a list of supported
    hash algorithms.

  * **-G**, **\--key-algorithm**=_KEY\_ALGORITHM_:

    The key algorithm associated with this object. It defaults to "rsa" if not
    specified.
    It accepts friendly names just like -g option.
    See section "Supported Public Object Algorithms" for a list
    of supported object algorithms. Mutually exclusive of **-i**.

  * **-a**, **\--attributes**=_ATTRIBUTES_:

    The object attributes, optional. Object attributes follow the specifications
    as outlined in "object attribute specifiers". The default for created objects is:

    `TPMA_OBJECT_SIGN_ENCRYPT|TPMA_OBJECT_DECRYPT|TPMA_OBJECT_FIXEDTPM|TPMA_OBJECT_FIXEDPARENT|TPMA_OBJECT_SENSITIVEDATAORIGIN|TPMA_OBJECT_USERWITHAUTH`

    When **-i** is specified for sealing, `TPMA_OBJECT_SIGN_ENCRYPT` and `TPMA_OBJECT_DECRYPT`
    are removed from the default attribute set.
    The algorithm is set in a way where the the object is only good for sealing and unsealing.
    I.e. one cannot use an object for sealing and cryptography
    operations.

  * **-i**, **\--sealing-input**=_FILE_:

    The data file to be sealed, optional. If file is -, read from stdin.
    When sealing data only the _TPM\_ALG\_KEYEDHASH_ algorithm with a NULL scheme is allowed.
    Thus, **-G** cannot be specified.

  * **-L**, **\--policy**=_POLICY\_FILE_:

    The input policy file, optional.

  * **-u**, **\--public**=_OUTPUT\_PUBLIC\_FILE_:

    The output file which contains the public portion of the created object, optional.

  * **-r**, **\--private**=_OUTPUT\_PRIVATE\_FILE_:

    The output file which contains the sensitive portion of the object, optional.

  * **-c**, **\--key-context**=_OUTPUT\_CONTEXT\_FILE_:

    The output file which contains the key context, optional. The key context is analogous to the context
    file produced by **tpm2_load**(1), however is generated via a **tpm2_createloaded**(1) command. This option
    can be used to avoid the normal **tpm2_create**(1) and **tpm2_load**(1) command sequences and do it all in one
    command, atomically.


[common options](common/options.md)

[common tcti options](common/tcti.md)

[context object format](common/ctxobj.md)

[authorization formatting](common/authorizations.md)

[supported hash algorithms](common/hash.md)

[supported public object algorithms](common/object-alg.md)

[algorithm specifiers](common/alg.md)

[object attribute specifiers](common/obj-attrs.md)

# EXAMPLES

## Setup

In order to create an object, we must first create a primary key as it's parent.
```bash
tpm2_createprimary -c primary.ctx
```

## Create an Object

This will create an object using all the default values and store the TPM sealed private
and public portions to the paths specified via -u and -r respectively. The tool defaults
to an RSA key.

```bash
tpm2_create -C primary.ctx -u obj.pub -r obj.priv
```

## Seal Data to the TPM

Outside of key objects, the TPM allows for small amounts of user specified data to be sealed to the
TPM.

```bash
echo "my sealed data" > seal.dat
tpm2_create -C primary.ctx -i seal.dat -u obj.pub -r obj.priv
```

## Create an EC Key Object and Load it to the TPM

Normally, when creating an object, only the public and private portions of the object
are returned and the caller needs to use tpm2_load(1) to load those public and private
portions to the TPM before being able to use the object. However, this can be
accomplished within this command as well.

```bash
tpm2_create -C primary.ctx -G ecc -u obj.pub -r obj.priv -c ecc.ctx
```

[returns](common/returns.md)

[footer](common/footer.md)
