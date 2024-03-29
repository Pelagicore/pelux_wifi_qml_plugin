#include "wifibackend.h"

#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusPendingReply>
#include <QDBusPendingCall>
#include <QDBusVariant>
#include <QTimer>

#include <QDebug>

#include "connectivitymodule.h"

WiFiBackend::WiFiBackend(QObject *parent) : WiFiBackendInterface(parent)
{
    qRegisterMetaType<QQmlPropertyMap*>();

    ConnectivityModule::registerTypes();

    m_timerToNotify.setInterval(500);
    m_timerToNotify.setSingleShot(true);
    QObject::connect(&m_timerToNotify, &QTimer::timeout, this, 
            [this]() {
                emit accessPointsChanged( accessPoints() );
            });
}


void WiFiBackend::initialize()
{
    m_errorString = "";
    emit errorStringChanged(m_errorString);

    connectSignalsHandler();
        
    getProperty("WiFiAvailable", 
            [this](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<void> reply = *watcher;
                QDBusMessage message = reply.reply();
                const QVariant var = message.arguments().first().value<QDBusVariant>().variant();
                setAvailable( var.toBool() );
                watcher->deleteLater();
            });

    getProperty("WiFiEnabled", 
            [this](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<void> reply = *watcher;
                QDBusMessage message = reply.reply();
                const QVariant var = message.arguments().first().value<QDBusVariant>().variant();
                setEnabled( var.toBool() );
                watcher->deleteLater();
            });

    getProperty("WiFiHotspotEnabled",
            [this](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<void> reply = *watcher;
                QDBusMessage message = reply.reply();
                const QVariant var = message.arguments().first().value<QDBusVariant>().variant();
                setHotspotEnabled( var.toBool() );
                watcher->deleteLater();
            });

    getProperty("WiFiHotspotSSID",
            [this](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<void> reply = *watcher;
                QDBusMessage message = reply.reply();
                const QVariant var = message.arguments().first().value<QDBusVariant>().variant();
                setHotspotSSID( var.toString() );
                watcher->deleteLater();
            });

    getProperty("WiFiHotspotPassphrase",
            [this](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<void> reply = *watcher;
                QDBusMessage message = reply.reply();
                const QVariant var = message.arguments().first().value<QDBusVariant>().variant();
                setHotspotPassword( var.toString() );
                watcher->deleteLater();
            });
    
    getProperty("WiFiAccessPoints",
            [this](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<void> reply = *watcher;
                QDBusMessage message = reply.reply();
                const QVariant var = message.arguments().first().value<QDBusVariant>().variant();
                const QDBusArgument arg = var.value<QDBusArgument>();
                m_dbusObjList.clear();
                arg.beginArray();
                while (!arg.atEnd()) {
                    QDBusObjectPath path;
                    arg >> path;
                    m_dbusObjList.append(path);
                }
                arg.endArray();
                updateAccessPoints();

                watcher->deleteLater();
            });

    emit connectionStatusChanged(m_connectionStatus);
    emit activeAccessPointChanged(m_activeAccessPoint);
    
    emit initializationDone();
}

QVariantList WiFiBackend::accessPoints() const
{
    //return m_accessPointObjects.values();
    QVariantList list;
    for (auto it = m_dbusObjList.constBegin(); it != m_dbusObjList.constEnd(); ++it) {
        QString path = (*it).path();
        if ( m_accessPointObjects.contains(path) ) {
            list.append( m_accessPointObjects.value(path) );
        }
    }
    return list;
}

void WiFiBackend::setAvailable(bool available)
{
    if (m_available == available)
        return;
    m_available = available;
    emit availableChanged(m_available);
}

void WiFiBackend::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    if( setProperty("WiFiEnabled", enabled) ) {
        m_enabled = enabled;
        emit enabledChanged(m_enabled);
    }

}

void WiFiBackend::setHotspotEnabled(bool hotspotEnabled)
{
    if (m_hotspotEnabled == hotspotEnabled)
        return;

    if( setProperty("WiFiHotspotEnabled", hotspotEnabled) ) {
        m_hotspotEnabled = hotspotEnabled;
        emit hotspotEnabledChanged(hotspotEnabled);
    }
}

void WiFiBackend::setHotspotSSID(const QString &hotspotSSID)
{
    if (m_hotspotSSID == hotspotSSID)
        return;

    QByteArray ssid = hotspotSSID.toUtf8();
    if ( ssid.at(ssid.length() - 1) != '\0' ) {
        ssid.append('\0');
    }
    if( setProperty("WiFiHotspotSSID", ssid) ) {
        m_hotspotSSID = hotspotSSID;
        emit hotspotSSIDChanged(hotspotSSID);
    }
}

void WiFiBackend::setHotspotPassword(const QString &hotspotPassword)
{
    if (m_hotspotPassword == hotspotPassword)
        return;

    if( setProperty("WiFiHotspotPassphrase", hotspotPassword) ) {
        m_hotspotPassword = hotspotPassword;
        emit hotspotPasswordChanged(hotspotPassword);
    }
}

void WiFiBackend::setAccessPoints(const QVariantList &accessPoints)
{
    Q_UNUSED(accessPoints)
    //TODO:implement ?
}

void WiFiBackend::setConnectionStatus(ConnectivityModule::ConnectionStatus connectionStatus)
{
    qWarning() << Q_FUNC_INFO << m_connectionStatus << connectionStatus;
    if (m_connectionStatus == connectionStatus)
        return;
    m_connectionStatus = connectionStatus;
    emit connectionStatusChanged(m_connectionStatus);
    qWarning() << Q_FUNC_INFO << "notified!";
}

void WiFiBackend::setActiveAccessPoint(const AccessPoint &activeAccessPoint)
{
    if (m_activeAccessPoint == activeAccessPoint)
        return;
    m_activeAccessPoint = activeAccessPoint;
    emit activeAccessPointChanged(m_activeAccessPoint);
}

void WiFiBackend::setErrorString(const QString &errorString)
{
    if (m_errorString == errorString)
        return;
    m_errorString = errorString;
    emit errorStringChanged(m_errorString);
}


void WiFiBackend::prepareUserInputAgent()
{
    if (!m_userInputAgent) {
        m_userInputAgent = new UserInputAgent(&m_dbusObject);
        QObject::connect(m_userInputAgent, &UserInputAgent::credentialsRequested, this, [this](const QString &ssid) {
                Q_EMIT WiFiBackend::credentialsRequested(ssid);
                });
    }
    WiFiBackend::dbusConnection().registerObject(userInputAgentDBusPath, userInputAgentDBusInterface, &m_dbusObject);
}

    
void WiFiBackend::destroyUserInputAgent()
{
}


QIviPendingReply<void> WiFiBackend::connectToAccessPoint(const QString &ssid)
{
    QIviPendingReply<void> reply;
    prepareUserInputAgent();

    QDBusMessage messageConnect = QDBusMessage::createMethodCall(connectivityDBusService, connectivityDBusPath, connectivityDBusInterface, "Connect" );

    QString objectPath;
    QMap<QString, QVariant>::const_iterator it;
    for (it = m_accessPointObjects.constBegin(); it != m_accessPointObjects.constEnd(); it++) {
        AccessPoint ap = it.value().value<AccessPoint>();
        if (ap.ssid() == ssid) {
            objectPath = it.key();
            break;
        }
    }
    
    if ( objectPath.isEmpty() ) {
        qWarning() << Q_FUNC_INFO << "Unknown SSID" << ssid << "to connect to.";
        reply.setFailed();
        return reply;
    }

    setActiveAccessPoint(AccessPoint(ssid, false, 0, ConnectivityModule::SecurityType::NoSecurity));
    setConnectionStatus(ConnectivityModule::Connecting);

    QVariantList args;
    args.append(QVariant::fromValue(QDBusObjectPath(objectPath)));
    args.append(QVariant::fromValue(QDBusObjectPath(userInputAgentDBusPath)));
    messageConnect.setArguments(args);

    QDBusPendingCall pendingCall = WiFiBackend::dbusConnection().asyncCall(messageConnect, ASYNC_CALL_TIMEOUT);
    QDBusPendingCallWatcher *pendingCallWatcher = new QDBusPendingCallWatcher(pendingCall, this);
    QObject::connect(pendingCallWatcher, &QDBusPendingCallWatcher::finished, this,
            [this, objectPath](QDBusPendingCallWatcher *watcher) {
                AccessPoint ap;
                QDBusPendingReply<void> reply = *watcher;
                if (reply.isError()) {
                    QString name = reply.error().name();
                    QString message = reply.error().message();
                    setErrorString(message);
                    qWarning() << Q_FUNC_INFO << name << ":" << message;
                    setConnectionStatus(ConnectivityModule::Disconnected);
                    setActiveAccessPoint(AccessPoint("", false, 0, ConnectivityModule::SecurityType::NoSecurity));
                }

                WiFiBackend::dbusConnection().unregisterObject(userInputAgentDBusPath);
                watcher->deleteLater();
            });
    
    reply.setSuccess();
    return reply;
}


QIviPendingReply<void> WiFiBackend::disconnectFromAccessPoint(const QString &ssid)
{
    QIviPendingReply<void> reply;

    if (connectionStatus() == ConnectivityModule::Connecting) {
        m_userInputAgent->cancel();
    }
    
    QDBusMessage messageConnect = QDBusMessage::createMethodCall(connectivityDBusService, connectivityDBusPath, connectivityDBusInterface, "Disconnect" );

    QString objectPath;
    QMap<QString, QVariant>::const_iterator it;
    for (it = m_accessPointObjects.constBegin(); it != m_accessPointObjects.constEnd(); it++) {
        AccessPoint ap = it.value().value<AccessPoint>();
        if (ap.ssid() == ssid) {
            objectPath = it.key();
            break;
        }
    }

    //if (objectPath.isEmpty()) {
    if (m_activeObjectPath.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "Unknown SSID" << ssid << "to disconnect to.";
        reply.setFailed();
        return reply;
    }

    QVariantList args;
    args.append(QVariant::fromValue(QDBusObjectPath(m_activeObjectPath)));
    messageConnect.setArguments(args);

    setConnectionStatus(ConnectivityModule::Disconnecting);

    QDBusPendingCall pendingCall = WiFiBackend::dbusConnection().asyncCall(messageConnect, ASYNC_CALL_TIMEOUT);
    QDBusPendingCallWatcher *pendingCallWatcher = new QDBusPendingCallWatcher(pendingCall, this);
    QObject::connect(pendingCallWatcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *watcher) {
                AccessPoint ap;
                QDBusPendingReply<void> reply = *watcher;
                if (reply.isError()) {
                    QString name = reply.error().name();
                    QString message = reply.error().message();
                    setErrorString(message);
                    qWarning() << Q_FUNC_INFO << name << ":" << message;
                    setConnectionStatus(ConnectivityModule::Disconnected);
                    //setActiveAccessPoint(AccessPoint("", false, 0, ConnectivityModule::SecurityType::NoSecurity));
                }
                watcher->deleteLater();
            });
    

    reply.setSuccess();
    return reply;
}

QIviPendingReply<void> WiFiBackend::sendCredentials(const QString &ssid, const QString &password)
{
    m_userInputAgent->sendCredentials(ssid, "", password);

    QIviPendingReply<void> reply;
    reply.setSuccess();
    return reply;
}
    

QDBusConnection WiFiBackend::dbusConnection()
{
    return QDBusConnection::systemBus();
}


void WiFiBackend::getProperty( const QString& propertyName, std::function<void(QDBusPendingCallWatcher*)> const& lambda)
{
    QDBusMessage dbusMessageRequestProperties = 
        QDBusMessage::createMethodCall(connectivityDBusService, connectivityDBusPath, dbusPropertyInterface, "Get" );
    QVariantList args;
    args.append(QVariant::fromValue( connectivityDBusInterface ));
    args.append(QVariant::fromValue( propertyName ));
    dbusMessageRequestProperties.setArguments(args);

    QDBusPendingCall pendingCall = WiFiBackend::dbusConnection().asyncCall(dbusMessageRequestProperties, ASYNC_CALL_TIMEOUT);
    QDBusPendingCallWatcher *pendingCallWatcher = new QDBusPendingCallWatcher(pendingCall, this);
      
    QObject::connect(pendingCallWatcher, &QDBusPendingCallWatcher::finished, this, lambda);
}

    
bool WiFiBackend::setProperty(const QString &propertyName, const QVariant &propertyValue)
{
    QDBusInterface dbusInterface(connectivityDBusService, connectivityDBusPath, connectivityDBusInterface,
                                    WiFiBackend::dbusConnection(), this
                                );

    if (!dbusInterface.isValid()) {
        qWarning() << Q_FUNC_INFO << dbusInterface.lastError().message();
        return false;
    }

    return dbusInterface.setProperty(propertyName.toLatin1().data(), propertyValue);
}


void WiFiBackend::updateAccessPoints()
{
    if (!m_allowedToUpdateList) {
        m_requiredUpdate = true;
        return;
    }

    m_allowedToUpdateList = false;

    QTimer::singleShot(700, [this]() {
            m_allowedToUpdateList = true;
            if (m_requiredUpdate) {
                m_requiredUpdate = false;
                getProperty("WiFiAccessPoints", [this](QDBusPendingCallWatcher *watcher) {
                    QDBusPendingReply<void> reply = *watcher;
                    QDBusMessage message = reply.reply();
                    const QVariant var = message.arguments().first().value<QDBusVariant>().variant();
                    const QDBusArgument arg = var.value<QDBusArgument>();
                    m_dbusObjList.clear();
                    arg.beginArray();
                    while (!arg.atEnd()) {
                        QDBusObjectPath path;
                        arg >> path;
                        m_dbusObjList.append(path);
                    }
                    arg.endArray();
                    updateAccessPoints();

                    watcher->deleteLater();
                });
            }
            });

    QMap<QString, bool> newAccessPoints;
    for (int i=0; i<m_dbusObjList.count(); i++) {
        QString dbusObjPath = m_dbusObjList.at(i).path();
        newAccessPoints.insert(dbusObjPath, true);

        if ( m_accessPointObjects.contains(dbusObjPath) ) {
            continue;
        }

        QDBusMessage dbusMessageRequestProperties = 
            QDBusMessage::createMethodCall(connectivityDBusService, dbusObjPath, dbusPropertyInterface, "GetAll" );
        QVariantList args;
        args.append(QVariant::fromValue( accessPointDBusInterface ));
        dbusMessageRequestProperties.setArguments(args);

        QDBusPendingCall pendingCall = WiFiBackend::dbusConnection().asyncCall(dbusMessageRequestProperties, ASYNC_CALL_TIMEOUT);
        QDBusPendingCallWatcher *pendingCallWatcher = new QDBusPendingCallWatcher(pendingCall, this);

        QObject::connect(pendingCallWatcher, &QDBusPendingCallWatcher::finished, this, 
                [this, dbusObjPath](QDBusPendingCallWatcher *watcher) {
                    QDBusPendingReply<void> reply = *watcher;

                    if (reply.isError()) {
                        qWarning() << Q_FUNC_INFO << reply.error().message();
                    } else {
                        QDBusMessage message = reply.reply();
                        const QDBusArgument arg = message.arguments().first().value<QDBusArgument>();
                        AccessPoint ap;
                        arg.beginMap();
                        while (!arg.atEnd()) {
                            arg.beginMapEntry();
                            QString propertyName;
                            QVariant propertyValue;
                            arg >> propertyName >> propertyValue;
                            if (propertyName == "SSID") {
                                ap.setSsid( propertyValue.toString() );
                            } else if (propertyName == "Connected") {
                                ap.setConnected( propertyValue.toBool() );
                            } else if (propertyName == "Strength") {
                                ap.setStrength( propertyValue.toInt() );
                            } else if (propertyName == "Security") {
                                ap.setSecurity( securityTypeString2Enum(propertyValue.toString()) );
                            }

                            arg.endMapEntry();
                        }
                        arg.endMap();

                        if (!ap.ssid().isEmpty()) {
                            if (ap.connected() && (connectionStatus() != ConnectivityModule::Connected)) {
                                setConnectionStatus(ConnectivityModule::Connected);
                                setActiveAccessPoint(ap);
                                m_activeObjectPath = dbusObjPath;
                            } else if ((connectionStatus() != ConnectivityModule::Disconnected) && (m_activeObjectPath == dbusObjPath) && (!ap.connected())) {
                                setConnectionStatus(ConnectivityModule::Disconnected);
                                setActiveAccessPoint(AccessPoint("", false, 0, ConnectivityModule::SecurityType::NoSecurity));
                                m_activeObjectPath = "";
                            }

                            m_accessPointObjects.insert(dbusObjPath, QVariant::fromValue(ap));
            
                            dbusConnection().connect(connectivityDBusService, dbusObjPath, dbusPropertyInterface, 
                                    QStringLiteral("PropertiesChanged"), this, SLOT(propertiesChangedHandler(QDBusMessage)));
    
                            m_timerToNotify.start();
                        }
                    }
                    watcher->deleteLater();
                });
    }

    // Remove unexisting access points
    bool someAPsRemoved = false;
    for (auto it = m_accessPointObjects.begin(); it != m_accessPointObjects.end();) {
        if ( !newAccessPoints.contains(it.key()) ) {
            dbusConnection().disconnect( connectivityDBusService, it.key(), dbusPropertyInterface,
                    QStringLiteral("PropertiesChanged"), this, SLOT(propertiesChangedHandler(QDBusMessage)));
            it = m_accessPointObjects.erase(it);
            someAPsRemoved = true;
        } else {
            ++it;
        }
    }

    if (someAPsRemoved) {
        m_timerToNotify.start();
    }
}


void WiFiBackend::connectSignalsHandler()
{
    if (m_dbusSignalsConnected) {
        return;
    }

    QDBusConnection conn = WiFiBackend::dbusConnection();
    
    m_dbusSignalsConnected = conn.connect(connectivityDBusService, connectivityDBusPath, dbusPropertyInterface, 
            QStringLiteral("PropertiesChanged"), this, SLOT(propertiesChangedHandler(QDBusMessage)));
}


void WiFiBackend::propertiesChangedHandler(const QDBusMessage &message)
{
    const QVariantList arguments = message.arguments();

    if (arguments.value(0) == connectivityDBusInterface) {
        const QDBusArgument& argument1 = arguments.value(1).value<QDBusArgument>();

        argument1.beginMap();
        while (!argument1.atEnd()) {
            argument1.beginMapEntry();
            QString propertyName;
            QVariant propertyValue;
            argument1 >> propertyName >> propertyValue;
            if (propertyName == "WiFiEnabled") {
                setEnabled( propertyValue.toBool() );
            } else if (propertyName == "WiFiAvailable") {
                setAvailable( propertyValue.toBool() );
            } else if (propertyName == "WiFiAccessPoints") {
                const QDBusArgument &arg = propertyValue.value<QDBusArgument>();
                m_dbusObjList.clear();
                arg.beginArray();
                while (!arg.atEnd()) {
                    QDBusObjectPath path;
                    arg >> path;
                    m_dbusObjList.append(path);
                }
                arg.endArray();
                updateAccessPoints();
            }
            argument1.endMapEntry();
        }
        argument1.endMap();
    } else if (arguments.value(0) == accessPointDBusInterface) {
    
        QString objectPath = message.path();
        if ( m_accessPointObjects.contains(objectPath) ) {
            AccessPoint ap = m_accessPointObjects.value(objectPath).value<AccessPoint>();
 
            const QDBusArgument argument1 = arguments.value(1).value<QDBusArgument>();
            argument1.beginMap();
            while (!argument1.atEnd()) {
                argument1.beginMapEntry();
                QString propertyName;
                QVariant propertyValue;
                argument1 >> propertyName >> propertyValue;
                if (propertyName == "SSID") {
                    ap.setSsid( propertyValue.toString() );
                } else if (propertyName == "Connected") {
                    ap.setConnected( propertyValue.toBool() );
                } else if (propertyName == "Strength") {
                    ap.setStrength( propertyValue.toInt() );
                } else if (propertyName == "Security") {
                    ap.setSecurity( securityTypeString2Enum(propertyValue.toString()) );
                }

                argument1.endMapEntry();
            }
            argument1.endMap();
                
            m_accessPointObjects.insert(objectPath, QVariant::fromValue(ap));
            if ((connectionStatus() != ConnectivityModule::Connected) && ap.connected() ) {
                setConnectionStatus(ConnectivityModule::Connected);
                setActiveAccessPoint(ap);
                m_activeObjectPath = objectPath;
            } else if ((connectionStatus() != ConnectivityModule::Disconnected) && (m_activeObjectPath == objectPath) && (!ap.connected())) {
                setConnectionStatus(ConnectivityModule::Disconnected);
                setActiveAccessPoint(AccessPoint("", false, 0, ConnectivityModule::SecurityType::NoSecurity));
                m_activeObjectPath = "";
            }
            m_timerToNotify.start();
        }
    }
}


ConnectivityModule::SecurityType WiFiBackend::securityTypeString2Enum(const QString& securityString)
{
    ConnectivityModule::SecurityType security = ConnectivityModule::SecurityType::NoSecurity;
    if (securityString == "wep") {
        security = ConnectivityModule::SecurityType::WEP;
    } else if (securityString == "wpa-psk") {
        security = ConnectivityModule::SecurityType::WPA_PSK;
    } else if (securityString == "wpa-eap") {
        security = ConnectivityModule::SecurityType::WPA_EAP;
    }
    return security;
}

