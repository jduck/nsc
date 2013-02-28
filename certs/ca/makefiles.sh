#!/bin/sh
#
# original script by Vladimir Kornea
#
# modified slightly by Joshua J. Drake
#

# make sure environment exists
[ ! -d ca.db.certs ] && mkdir ca.db.certs
[ ! -f ca.db.serial ] && echo '01' >ca.db.serial
[ ! -f ca.db.index ] && cp /dev/null ca.db.index


# create an own SSLeay config
cat > ca.config << EOT
[ ca ]
default_ca = CA_own
[ CA_own ]
dir = .
certs = \$dir
new_certs_dir = \$dir/ca.db.certs
database = \$dir/ca.db.index
serial = \$dir/ca.db.serial
RANDFILE = \$dir/ca.db.rand
certificate = \$dir/ca.crt
private_key = \$dir/ca.key
default_days = 365
default_crl_days = 30
default_md = md5
preserve = no
policy = policy_anything
[ policy_anything ]
countryName = optional
stateOrProvinceName = optional
localityName = optional
organizationName = optional
organizationalUnitName = optional
commonName = supplied
emailAddress = optional
EOT
