/*************************************************************************
 * Copyright 2009-2012 Eucalyptus Systems, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 *
 * Please contact Eucalyptus Systems, Inc., 6755 Hollister Ave., Goleta
 * CA 93117, USA or visit http://www.eucalyptus.com/licenses/ if you need
 * additional information or have any questions.
 ************************************************************************/

package com.eucalyptus.cluster.callback;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Set;
import java.util.concurrent.ConcurrentMap;

import com.eucalyptus.cloudwatch.CloudWatch;
import com.eucalyptus.cloudwatch.Dimension;
import com.eucalyptus.cloudwatch.Dimensions;
import com.eucalyptus.cloudwatch.MetricData;
import com.eucalyptus.cloudwatch.MetricDatum;
import com.eucalyptus.cloudwatch.PutMetricDataType;
import com.eucalyptus.cloudwatch.PutMetricDataResponseType;

import com.eucalyptus.util.EucalyptusCloudException;
import edu.ucsb.eucalyptus.msgs.BaseMessage;
import edu.ucsb.eucalyptus.msgs.DescribeSensorsResponse;
import edu.ucsb.eucalyptus.msgs.DescribeSensorsType;
import edu.ucsb.eucalyptus.msgs.MetricCounterType;
import edu.ucsb.eucalyptus.msgs.MetricDimensionsType;
import edu.ucsb.eucalyptus.msgs.MetricsResourceType;
import edu.ucsb.eucalyptus.msgs.SensorsResourceType;
import edu.ucsb.eucalyptus.msgs.MetricDimensionsValuesType;

import org.apache.log4j.Logger;

import com.eucalyptus.auth.Accounts;
import com.eucalyptus.auth.AuthException;
import com.eucalyptus.auth.principal.Account;
import com.eucalyptus.auth.principal.User;
import com.eucalyptus.component.ComponentIds;
import com.eucalyptus.component.ServiceConfiguration;
import com.eucalyptus.component.ServiceConfigurations;
import com.eucalyptus.event.EventFailedException;
import com.eucalyptus.event.ListenerRegistry;
import com.eucalyptus.records.Logs;
import com.eucalyptus.reporting.event.InstanceUsageEvent;
import com.eucalyptus.util.LogUtil;
import com.eucalyptus.util.async.AsyncRequests;
import com.eucalyptus.util.async.BroadcastCallback;
import com.eucalyptus.vm.VmInstance.VmState;
import com.eucalyptus.vm.VmInstance;
import com.eucalyptus.vm.VmInstances;

import com.google.common.base.Function;
import com.google.common.base.Supplier;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Iterables;
import com.google.common.collect.Lists;
import com.google.common.collect.Maps;
import com.google.common.collect.Ordering;

public class DescribeSensorCallback extends
    BroadcastCallback<DescribeSensorsType, DescribeSensorsResponse> {

  private static final Logger LOG = Logger.getLogger(DescribeSensorCallback.class);
  private static final String RESOURCE_TYPE_INSTANCE = "instance";
  private final int historySize;
  private final int collectionIntervalTimeMs;
  private final ArrayList<String> instanceIds;
  private final ListenerRegistry listener = ListenerRegistry.getInstance();

  public DescribeSensorCallback(final int historySize,
                                final int collectionIntervalTimeMS, final ArrayList<String> instanceIds) {
    this.historySize = historySize;
    this.collectionIntervalTimeMs = collectionIntervalTimeMS;
    this.instanceIds = instanceIds;

    final DescribeSensorsType msg =
        new DescribeSensorsType(this.historySize, this.collectionIntervalTimeMs, this.instanceIds);

    try {
      msg.setUser(Accounts.lookupSystemAdmin());
    } catch (AuthException e) {
      LOG.error("Unable to find the system user", e);
    }

    this.setRequest(msg);
  }

  @Override
  public void initialize(final DescribeSensorsType msg) {
  }

  @Override
  public BroadcastCallback<DescribeSensorsType, DescribeSensorsResponse> newInstance() {
    return new DescribeSensorCallback(this.historySize, this.collectionIntervalTimeMs, this.instanceIds);
  }

  @Override
  public void fireException(Throwable e) {
    LOG.debug("Request failed: "
        + LogUtil.subheader(this.getRequest().toString(
        "eucalyptus_ucsb_edu")));
    Logs.extreme().error(e, e);
  }

  private static class DiskReadWriteMetricTypeCache {
    
    private Map<String, MetricDimensionsValuesType> eventMap = Maps.newConcurrentMap();
    private String mapKey(SensorsResourceType sensorData,
        MetricDimensionsType dimensionType, MetricDimensionsValuesType value) {

      String SEPARATOR = "|";
      // sensor data should include resource Uuid and resource name
      String resourceUUID = (sensorData != null) ? sensorData.getResourceUuid() : null;
      String resourceName = (sensorData != null) ? sensorData.getResourceName() : null;
      // dimension type should include dimension name
      String dimensionName = (dimensionType != null) ? dimensionType.getDimensionName() : null;
      // value should include timestamp
      String valueTimestampStr = (value != null && value.getTimestamp() != null) ? value.getTimestamp().toString() : null;
      return resourceUUID + SEPARATOR + resourceName + SEPARATOR + dimensionName + SEPARATOR + valueTimestampStr;
      
      
    }
    public void putEventInCache(SensorsResourceType sensorData,
        MetricDimensionsType dimensionType, MetricDimensionsValuesType value) {
      eventMap.put(mapKey(sensorData, dimensionType, value), value);
    }

    public MetricDimensionsValuesType getEventFromCache(
        SensorsResourceType sensorData, MetricDimensionsType dimensionType,
        MetricDimensionsValuesType value) {
      return eventMap.get(mapKey(sensorData, dimensionType, value));
    }
  }
  
  @Override
  public void fire(final DescribeSensorsResponse msg) {
    try {
      final Iterable<String> uuidList =
          Iterables.transform(VmInstances.list(VmState.RUNNING), VmInstances.toInstanceUuid());
      LOG.debug("DescribeSensorCallback (fire) called at " + new Date());

      // cloudwatch metric caches
      final ConcurrentMap<String, DiskReadWriteMetricTypeCache> metricCacheMap = Maps.newConcurrentMap();

      
      
      for (final SensorsResourceType sensorData : msg.getSensorsResources()) {
        if (!RESOURCE_TYPE_INSTANCE.equals(sensorData.getResourceType()) ||
            !Iterables.contains(uuidList, sensorData.getResourceUuid()))
          continue;
        
        for (final MetricsResourceType metricType : sensorData.getMetrics()) {
          for (final MetricCounterType counterType : metricType.getCounters()) {
            for (final MetricDimensionsType dimensionType : counterType.getDimensions()) {

              // find and fire most recent value for metric/dimension
              final List<MetricDimensionsValuesType> values =
                  Lists.newArrayList(dimensionType.getValues());

              //CloudWatch use case of metric data

              if(!values.isEmpty()) {
                // best to enter older data first...
                Collections.sort(values, Ordering.natural().onResultOf(GetTimestamp.INSTANCE));

                for (MetricDimensionsValuesType value : values) {
                  LOG.debug("ResourceUUID: " + sensorData.getResourceUuid());
                  LOG.debug("ResourceName: " + sensorData.getResourceName());
                  LOG.debug("Metric: " + metricType.getMetricName());
                  LOG.debug("Dimension: " + dimensionType.getDimensionName());
                  LOG.debug("Timestamp: " + value.getTimestamp());
                  LOG.debug("Value: " + value.getValue());
                  final Long currentTimeStamp = value.getTimestamp().getTime();
                  final Double currentValue = value.getValue();
                  sendSystemMetric(new Supplier<InstanceUsageEvent>() {
                    @Override
                    public InstanceUsageEvent get() {
                      return new InstanceUsageEvent(
                          sensorData.getResourceUuid(),
                          sensorData.getResourceName(),
                          metricType.getMetricName(),
                          dimensionType.getSequenceNum(),
                          dimensionType.getDimensionName(),
                          currentValue,
                          currentTimeStamp);
                    }
                  });
                  
                  // special case to calculate VolumeConsumedReadWriteOps
                  // As it is (VolumeThroughputPercentage / 100) * (VolumeReadOps + VolumeWriteOps), and we are hard coding
                  // VolumeThroughputPercentage as 100%, we will just use VolumeReadOps + VolumeWriteOps
                  
                  // And just in case VolumeReadOps is called DiskReadOps we do both cases...
                  combineReadWriteDiskMetric("DiskReadOps", "DiskWriteOps", metricCacheMap, "DiskConsumedReadWriteOps", metricType, sensorData, dimensionType, value);
                  combineReadWriteDiskMetric("VolumeReadOps", "VolumeWriteOps", metricCacheMap, "VolumeConsumedReadWriteOps", metricType, sensorData, dimensionType, value);

                  // Also need VolumeTotalReadWriteTime to compute VolumeIdleTime
                  combineReadWriteDiskMetric("VolumeTotalReadTime", "VolumeTotalWriteTime", metricCacheMap, "VolumeTotalReadWriteTime", metricType, sensorData, dimensionType, value);
                }
              }

              //Reporting use case of metric data from the cc
              Collections.sort(values, Ordering.natural().onResultOf(GetTimestamp.INSTANCE));

              if (!values.isEmpty()) {
                final MetricDimensionsValuesType latestValue = Iterables.getLast(values);
                final Double usageValue = latestValue.getValue();
                final Long usageTimestamp = latestValue.getTimestamp().getTime();
                final long sequenceNumber = dimensionType.getSequenceNum() + (values.size() - 1);
                fireUsageEvent( new Supplier<InstanceUsageEvent>(){
                  @Override
                  public InstanceUsageEvent get() {
                    return new InstanceUsageEvent(
                        sensorData.getResourceUuid(),
                        sensorData.getResourceName(),
                        metricType.getMetricName(),
                        sequenceNumber,
                        dimensionType.getDimensionName(),
                        usageValue,
                        usageTimestamp);
                  }
                });
              }
            }
          }
        }
      }
    } catch (Exception ex) {
      LOG.debug("Unable to fire describe sensors call back", ex);
    }
  }

  private void combineReadWriteDiskMetric(String readMetricName, String writeMetricName,
      ConcurrentMap<String, DiskReadWriteMetricTypeCache> metricCacheMap,
      String combinedMetricName, MetricsResourceType metricType,
      SensorsResourceType sensorData, MetricDimensionsType dimensionType,
      MetricDimensionsValuesType thisValueType) throws Exception {
    metricCacheMap.putIfAbsent(readMetricName, new DiskReadWriteMetricTypeCache());
    metricCacheMap.putIfAbsent(writeMetricName, new DiskReadWriteMetricTypeCache());
    
    String matchingMetricName = null;
    String otherMetricName = null;
    if (metricType.getMetricName().equals(readMetricName)) {
      matchingMetricName = readMetricName;
      otherMetricName = writeMetricName;
    } else if (metricType.getMetricName().equals(writeMetricName)) {
      matchingMetricName = writeMetricName;
      otherMetricName = readMetricName;
    }
    if (matchingMetricName != null && otherMetricName != null) {
      metricCacheMap.get(matchingMetricName).putEventInCache(sensorData, dimensionType, thisValueType);
      MetricDimensionsValuesType otherValueType = metricCacheMap.get(otherMetricName).getEventFromCache(sensorData, dimensionType, thisValueType);
      if (otherValueType != null) {
        sendSystemMetric(createDiskOpsCacheSupplier(
          sensorData, 
        combinedMetricName, 
        dimensionType,
        thisValueType.getValue() + otherValueType.getValue(),
        thisValueType.getTimestamp().getTime()));
      }
    }
  }

  private Supplier<InstanceUsageEvent> createDiskOpsCacheSupplier(
      final SensorsResourceType sensorData, final String combinedMetricName,
      final MetricDimensionsType dimensionType, final Double value,
      final Long usageTimeStamp) {
    
    return new Supplier<InstanceUsageEvent>(){
      @Override
      public InstanceUsageEvent get() {
        return new InstanceUsageEvent(
            sensorData.getResourceUuid(),
            sensorData.getResourceName(),
            combinedMetricName,
            dimensionType.getSequenceNum(),
            dimensionType.getDimensionName(),
            value,
            usageTimeStamp);
      }
    };
  }

  private void fireUsageEvent(Supplier<InstanceUsageEvent> instanceUsageEventSupplier) {
    InstanceUsageEvent event = null;
    event = instanceUsageEventSupplier.get();
    try {
      listener.fireEvent(event);
    } catch (EventFailedException e) {
      LOG.debug("Failed to fire instance usage event"
          + (event != null ? event : ""), e);
    }
  }

  private enum GetTimestamp implements Function<MetricDimensionsValuesType, Date> {
    INSTANCE;

    @Override
    public Date apply(final MetricDimensionsValuesType metricDimensionsValuesType) {
      return metricDimensionsValuesType.getTimestamp();
    }
  }

  private static final Set<String> UNSUPPORTED_EC2_METRICS = ImmutableSet.of(
      "VolumeQueueLength", 
      "VolumeTotalReadTime", 
      "VolumeTotalWriteTime", 
      "VolumeTotalReadWriteTime", 
      "VolumeConsumedReadWriteOps",
      "DiskTotalReadTime", 
      "DiskTotalWriteTime", 
      "DiskConsumedReadWriteOps");

  private static final Map<String, String> ABSOLUTE_METRICS = 
      new ImmutableMap.Builder<String, String>()
      .put("CPUUtilization", "CPUUtilizationMSAbsolute") // this is actually the data in milliseconds, not percentage
      .put("VolumeReadOps", "VolumeReadOpsAbsolute") // this is actually the total volume read Ops since volume creation, not for the period
      .put("VolumeWriteOps", "VolumeWriteOpsAbsolute") // this is actually the total volume write Ops since volume creation, not for the period
      .put("VolumeConsumedReadWriteOps", "VolumeConsumedReadWriteOpsAbsolute") // this is actually the total volume consumed read write Ops since volume creation, not for the period
      .put("VolumeReadBytes", "VolumeReadBytesAbsolute") // this is actually the total volume read bytes since volume creation, not for the period
      .put("VolumeWriteBytes", "VolumeWriteBytesAbsolute") // this is actually the total volume write bytes since volume creation, not for the period
      .put("VolumeTotalReadTime", "VolumeTotalReadTimeAbsolute") // this is actually the total volume read time since volume creation, not for the period
      .put("VolumeTotalWriteTime", "VolumeTotalWriteTimeAbsolute") // this is actually the total volume read and write time since volume creation, not for the period
      .put("VolumeTotalReadWriteTime", "VolumeTotalReadWriteTimeAbsolute") // this is actually the total volume read and write time since volume creation, not for the period
      .put("DiskReadOps", "DiskReadOpsAbsolute") // this is actually the total disk read Ops since instance creation, not for the period
      .put("DiskWriteOps", "DiskWriteOpsAbsolute") // this is actually the total disk write Ops since instance creation, not for the period
      .put("DiskReadBytes", "DiskReadBytesAbsolute") // this is actually the total disk read bytes since instance creation, not for the period
      .put("DiskWriteBytes", "DiskWriteBytesAbsolute") // this is actually the total disk write bytes since instance creation, not for the period
      .put("NetworkIn", "NetworkInAbsolute") // this is actually the total network in bytes since instance creation, not for the period
      .put("NetworkInExternal", "NetworkInExternalAbsolute") // this is actually the total network in external bytes since instance creation, not for the period
      .put("NetworkOut", "NetworkOutAbsolute") // this is actually the total network out bytes since instance creation, not for the period
      .put("NetworkOutExternal", "NetworkOutExternalAbsolute") // this is actually the total network out external bytes since instance creation, not for the period
      .build();

  private void sendSystemMetric(Supplier<InstanceUsageEvent> cloudWatchSupplier) throws Exception {
    InstanceUsageEvent event = null;
    event = cloudWatchSupplier.get();

    final VmInstance instance = VmInstances.lookup(event.getInstanceId());

    if (!instance.getInstanceId().equals(event.getInstanceId())
        || !instance.getMonitoring()) {
      throw new NoSuchElementException("Instance : " + event.getInstanceId() + " monitoring is not enabled");
    }

    if (instance.getInstanceId().equals(event.getInstanceId())
        && instance.getMonitoring()) {

      PutMetricDataType putMetricData = new PutMetricDataType();
      MetricDatum metricDatum = new MetricDatum();
      ArrayList<Dimension> dimArray = Lists.newArrayList();

      if (event.getDimension() != null && event.getValue() != null) {

        if (event.getDimension().startsWith("vol-")) {
          putMetricData.setNamespace("AWS/EBS");
          Dimension volDim = new Dimension();
          volDim.setName("VolumeId");
          volDim.setValue(event.getDimension());
          dimArray.add(volDim);
          // Need to replace metric name
          if (event.getMetric().startsWith("Disk")) {
            final String convertedEBSMetricName = event.getMetric()
                .replace("Disk", "Volume");
            metricDatum.setMetricName(convertedEBSMetricName);
          } else {
            metricDatum.setMetricName(event.getMetric());
          }
        } else {
          putMetricData.setNamespace("AWS/EC2");

          Dimension instanceIdDim = new Dimension();
          instanceIdDim.setName("InstanceId");
          instanceIdDim.setValue(instance.getInstanceId());
          dimArray.add(instanceIdDim);

          Dimension imageIdDim = new Dimension();
          imageIdDim.setName("ImageId");
          imageIdDim.setValue(instance.getImageId());
          dimArray.add(imageIdDim);

          Dimension instanceTypeDim = new Dimension();
          instanceTypeDim.setName("InstanceType");
          instanceTypeDim.setValue(instance.getVmType()
              .getDisplayName());
          dimArray.add(instanceTypeDim);
          // convert ephemeral disks metrics
          if (UNSUPPORTED_EC2_METRICS.contains(event.getMetric())) {
            return;
          } else {
            metricDatum.setMetricName(event.getMetric());
          }
        }
      } else {
        LOG.debug("Event does not contain a dimension");
        throw new Exception();
      }

      Dimensions dims = new Dimensions();
      dims.setMember(dimArray);

      MetricData metricData = new MetricData();

      metricDatum.setTimestamp(new Date(event.getValueTimestamp()));
      metricDatum.setDimensions(dims);
      metricDatum.setValue(event.getValue());

      final String unitType = containsUnitType(metricDatum.getMetricName());
      metricDatum.setUnit(unitType);

      if (ABSOLUTE_METRICS.containsKey(metricDatum.getMetricName())) {
        metricDatum.setMetricName(ABSOLUTE_METRICS.get(metricDatum.getMetricName()));
      }
      
      metricData.setMember(Lists.newArrayList(metricDatum));
      putMetricData.setMetricData(metricData);

      Account account = Accounts.getAccountProvider().lookupAccountById(
          instance.getOwnerAccountNumber());

      User user = account.lookupUserByName(User.ACCOUNT_ADMIN);
      putMetricData.setEffectiveUserId(user.getUserId());

      ServiceConfiguration serviceConfiguration = ServiceConfigurations
          .createEphemeral(ComponentIds.lookup(CloudWatch.class));
      BaseMessage reply = (BaseMessage) AsyncRequests.dispatch(serviceConfiguration, putMetricData).get();
      if (!(reply instanceof PutMetricDataResponseType)) {
        throw new EucalyptusCloudException("Unable to send put metric data to cloud watch");
      }

    }
  }

  private enum Bytes {
    VolumeReadBytes,
    VolumeWriteBytes,
    DiskReadBytes,
    DiskWriteBytes,
    NetworkIn,
    NetworkInExternal,
    NetworkOut,
    NetworkOutExternal
  }

  private enum Count {
    VolumeWriteOps,
    VolumeQueueLength,
    VolumeConsumedReadWriteOps,
    DiskReadOps,
    DiskWriteOps,
    StatusCheckFailed,
    VolumeReadOps
  }

  private enum Seconds {
    VolumeTotalReadTime,
    VolumeTotalWriteTime,
    VolumeTotalReadWriteTime,
    VolumeIdleTime
  }

  private enum Percent {
    VolumeThroughputPercentage,
    CPUUtilization
  }

  private String containsUnitType(final String metricType) {
    //TODO:KEN find cleaner method of finding the metric type
    try {
      Enum.valueOf(Bytes.class, metricType);
      return "Bytes";
    } catch (IllegalArgumentException ex1) {
      try {
        Enum.valueOf(Count.class, metricType);
        return "Count";
      } catch (IllegalArgumentException ex2) {
        try {
          Enum.valueOf(Seconds.class, metricType);
          return "Seconds";
        } catch (IllegalArgumentException ex4) {
          try {
            Enum.valueOf(Percent.class, metricType);
            return "Percent";
          } catch (IllegalArgumentException ex5) {
            throw new NoSuchElementException(
                "Unknown system unit type : " + metricType);
          }
        }
      }
    }
  }

}
