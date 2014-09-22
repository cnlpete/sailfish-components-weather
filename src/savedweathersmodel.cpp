#include "savedweathersmodel.h"
#include <QDir>
#include <qqmlinfo.h>
#include <QStandardPaths>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static QString weatherStoragePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/sailfish-weather/");
}

SavedWeathersModel::SavedWeathersModel(QObject *parent)
    : QAbstractListModel(parent), m_currentWeather(0), m_autoRefresh(false),
      m_metric(true), m_fileWatcher(0)
{
    load();
}

SavedWeathersModel::~SavedWeathersModel()
{
}

void SavedWeathersModel::load()
{
    QFile file(weatherStoragePath() + QStringLiteral("/weather.json"));
    if (!file.exists())
        return;

    if (!file.open(QIODevice::ReadOnly)) {
        qmlInfo(this) << "Could not open weather data file!";
        return;
    }

    QList <int> locationIds;
    int oldCount = m_savedWeathers.count();

    QByteArray data = file.readAll();
    QJsonDocument json = QJsonDocument::fromJson(data);

    QJsonObject root = json.object();

    QJsonValue metricValue = root.value("metric");
    if (metricValue.type() != QJsonValue::Undefined) {
        setMetric(metricValue.toBool());
    }

    // update current weather locations
    QJsonObject currentLocation = root.value("currentLocation").toObject();
    if (!currentLocation.empty()) {
        setCurrentWeather(currentLocation.toVariantMap(), true /* internal */);
        QVariantMap weatherMap = currentLocation.value("weather").toObject().toVariantMap();
        m_currentWeather->update(weatherMap);
        m_currentWeather->setStatus(Weather::Status(weatherMap["status"].toInt()));
    }

    // update saved weather locations
    QJsonArray savedLocations = root.value("savedLocations").toArray();
    foreach (const QJsonValue &value, savedLocations) {
        QJsonObject location = value.toObject();
        int locationId = location["locationId"].toInt();

        locationIds.append(locationId);
        // add new weather locations
        if (getWeatherIndex(locationId) < 0) {
            addLocation(location.toVariantMap());
        }
        QVariantMap weatherMap = location.value("weather").toObject().toVariantMap();
        // update existing weather locations
        update(locationId, weatherMap, Weather::Status(weatherMap["status"].toInt()));
    }

    // remove old weather locations
    for (int i = 0; i < m_savedWeathers.count(); i++) {
        Weather *weather = m_savedWeathers[i];
        if (!locationIds.contains(weather->locationId())) {
            beginRemoveRows(QModelIndex(), i, i);
            m_savedWeathers.removeAt(i);
            endRemoveRows();
        }
    }
    if (m_savedWeathers.count() != oldCount) {
        emit countChanged();
    }
}

Q_INVOKABLE void SavedWeathersModel::moveToTop(int index)
{
    if (index > 0) {
        beginMoveRows(QModelIndex(), index, index, QModelIndex(), 0);
        m_savedWeathers.move(index, 0);
        endMoveRows();
        save();
    }
}

void SavedWeathersModel::save()
{
    QJsonArray savedLocations;
    foreach (Weather *weather, m_savedWeathers) {
        savedLocations.append(convertToJson(weather));
    }

    QJsonObject root;
    root.insert("metric", QJsonValue(m_metric));
    if (m_currentWeather) {
        root.insert("currentLocation", convertToJson(m_currentWeather));
    }
    root.insert("savedLocations", savedLocations);

    QJsonDocument json(root);

    QDir dir(weatherStoragePath());
    if (!dir.mkpath(QStringLiteral("."))) {
        qmlInfo(this) << "Could not create data directory!";
        return;
    }

    QFile file(dir.filePath(QStringLiteral("weather.json")));
    if (!file.open(QIODevice::WriteOnly)) {
        qmlInfo(this) << "Could not open weather data file!";
        return;
    }

    if (!file.write(json.toJson()) < 0) {
        qmlInfo(this) << "Could not write weather data:" << file.errorString();
        return;
    }
}
QJsonObject SavedWeathersModel::convertToJson(const Weather *weather)
{
    QJsonObject location;
    location["locationId"] = weather->locationId();
    location["city"] = weather->city();
    location["state"] = weather->state();
    location["country"] = weather->country();

    QJsonObject weatherData;
    weatherData["status"] = weather->status();
    weatherData["temperature"] = weather->temperature();
    weatherData["temperatureFeel"] = weather->temperatureFeel();
    weatherData["weatherType"] = weather->weatherType();
    weatherData["description"] = weather->description();
    weatherData["timestamp"] = weather->timestamp().toUTC().toString(Qt::ISODate);

    location["weather"] = weatherData;
    return location;
}

void SavedWeathersModel::addLocation(const QVariantMap &locationMap)
{
    int locationId = locationMap["locationId"].toInt();
    int i = getWeatherIndex(locationId);
    if (i >= 0) {
        qmlInfo(this) << "Location already exists " << locationId;
        return;
    }

    beginInsertRows(QModelIndex(), m_savedWeathers.count(), m_savedWeathers.count());

    Weather *weather = new Weather(this, locationMap);
    m_savedWeathers.append(weather);
    endInsertRows();
    emit countChanged();
}

void SavedWeathersModel::setCurrentWeather(const QVariantMap &map, bool internal)
{
    if (!m_currentWeather || m_currentWeather->locationId() != map["locationId"].toInt()
            // location API can return different place names, but the same weather station location id
            || m_currentWeather->city() != map["city"].toString()) {
        Weather *weather = new Weather(this, map);
        if (map.contains("temperature")) {
            weather->update(map);
            weather->setStatus(Weather::Ready);
        }
        if (m_currentWeather) {
            delete m_currentWeather;
        }
        m_currentWeather = weather;
        emit currentWeatherChanged();
        if (!internal) {
            save();
        }
    }
}

void SavedWeathersModel::reportError(int locationId)
{
    int i = getWeatherIndex(locationId);
    if (i < 0) {
        qmlInfo(this) << "No location with id " << locationId << " exists";
        return;
    }
    Weather *weather = m_savedWeathers[i];
    weather->setStatus(Weather::Error);
    dataChanged(index(i), index(i));
}

void SavedWeathersModel::update(int locationId, const QVariantMap &weatherMap, Weather::Status status)
{
    bool updatedCurrent = false;
    if (m_currentWeather && locationId == m_currentWeather->locationId()) {
        m_currentWeather->update(weatherMap);
        m_currentWeather->setStatus(status);
        updatedCurrent = true;
    }
    int i = getWeatherIndex(locationId);
    if (i < 0) {
        if (!updatedCurrent) {
            qmlInfo(this) << "Location hasn't been saved " << locationId;
        }
        return;
    }
    Weather *weather = m_savedWeathers[i];
    weather->update(weatherMap);
    weather->setStatus(status);
    dataChanged(index(i), index(i));
}

void SavedWeathersModel::remove(int locationId)
{
    int i = getWeatherIndex(locationId);
    if (i >= 0) {
        beginRemoveRows(QModelIndex(), i, i);
        m_savedWeathers.removeAt(i);
        endRemoveRows();
        emit countChanged();
    }
}

int SavedWeathersModel::count() const
{
    return rowCount();
}

Weather *SavedWeathersModel::currentWeather() const
{
    return m_currentWeather;
}

Weather *SavedWeathersModel::get(int locationId)
{
    int index = getWeatherIndex(locationId);
    if (index >= 0) {
        return m_savedWeathers.at(index);
    } else {
        qmlInfo(this) << "SavedWeathersModel::get(locationId) - no location with id " << locationId << " stored";
        return 0;
    }
}


int SavedWeathersModel::rowCount(const QModelIndex &) const
{
    return m_savedWeathers.count();
}

QVariant SavedWeathersModel::data(const QModelIndex &index, int role) const
{
    const Weather *weather = m_savedWeathers.at(index.row());
    switch (role) {
    case LocationId:
        return weather->locationId();
    case Status:
        return weather->status();
    case City:
        return weather->city();
    case State:
        return weather->state();
    case Country:
        return weather->country();
    case Temperature:
        return weather->temperature();
    case TemperatureFeel:
        return weather->temperatureFeel();
    case WeatherType:
        return weather->weatherType();
    case Description:
        return weather->description();
    case Timestamp:
        return weather->timestamp();
    case Populated:
        return weather->populated();
    }

    return QVariant();
}

QHash<int, QByteArray> SavedWeathersModel::roleNames() const
{
    QHash<int,QByteArray> roles;
    roles.insert(LocationId, "locationId");
    roles.insert(Status, "status");
    roles.insert(City, "city");
    roles.insert(State, "state");
    roles.insert(Country, "country");
    roles.insert(Temperature, "temperature");
    roles.insert(TemperatureFeel, "temperatureFeel");
    roles.insert(WeatherType, "weatherType");
    roles.insert(Description, "description");
    roles.insert(Timestamp, "timestamp");
    roles.insert(Populated, "populated");

    return roles;
}

int SavedWeathersModel::getWeatherIndex(int locationId)
{
    for (int i = 0; i < m_savedWeathers.count(); i++) {
        if (m_savedWeathers[i]->locationId() == locationId) {
            return i;
        }
    }
    return -1;
}

bool SavedWeathersModel::autoRefresh() const
{
    return m_autoRefresh;
}

void SavedWeathersModel::setAutoRefresh(bool enabled)
{
    if (m_autoRefresh == enabled)
        return;

    m_autoRefresh = enabled;

    if (m_autoRefresh) {
        QString filePath = weatherStoragePath() + QStringLiteral("weather.json");
        if (!QFile::exists(filePath)) {
            // QFileSystemWatcher needs the file to exist, so write out an
            // empty file
            save();
        }

        m_fileWatcher = new QFileSystemWatcher(this);
        connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
                this, &SavedWeathersModel::load);
        m_fileWatcher->addPath(filePath);
    } else {
        delete m_fileWatcher;
        m_fileWatcher = 0;
    }
}

bool SavedWeathersModel::metric() const
{
    return m_metric;
}

void SavedWeathersModel::setMetric(bool metric)
{
    if (m_metric != metric) {
        m_metric = metric;
        emit metricChanged();
    }
}
