Instruction for Debian PPA

1) copy debian template to project dir base
   cp -r AlsaJsonGateway/utils/debian  AlsaJsonGateway/debian

2) edit debian changelog
   + update your email and target OS
   + update version

3) check you build package locally
   + debuild -S -sa  # source package
   + debuild         # binaries

4) create your PGP signature [https://fedoraproject.org/wiki/Creating_GPG_Keys]
   + gpg --gen-key
   + select RSA (sign only)
   note: keys are store in ~/.gnupg

5) register your public key to targeted PPA
   + gpg --list-secret-keys
   + https://launchpad.net/USERID/+editpgpkeys

6) build a sign debian source package
   + export GPGKEY=xxxxxxx         # select the key you upload to PPA
   + debuild -k${GPGKEY} -S -sa    # warning no space after -k

6) upload your debian source package
   + dput -f ppa:fulup/alsa-json alsa-json-gateway_0.2-3_source.changes

WARNING:
   If dput looks working but nothing happen after and especially you do not receive a mail from PPA after dput.
   Double check that the key used for signing DEB source package is the same as the one declare in PPA.

   Look like you can only produce for one given version of Ubuntu with a given version of a package.

