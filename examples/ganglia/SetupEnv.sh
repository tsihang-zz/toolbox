

# INSTALL GANGLIA and ALL ITS DEPENDENCIES
# Choose "yes" when restarting apache2 required.
apt-get update
apt-get install rrdtool apache2 php ganglia-monitor gmetad ganglia-webfrontend

# PREPARE GANGLIA WEBFRONTEND APACHE ENVIRONMENT
cp /etc/ganglia-webfrontend/apache.conf /etc/apache2/sites-enabled/ganglia.conf


ln -s /usr/share/ganglia-webfrontend/ /var/www/ganglia

echo "edit gmetad.conf & gmond.conf in /etc/ganglia/"

