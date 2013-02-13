
ccflags-y += -Idrivers/media/dvb/dvb-core/
ccflags-y += -Idrivers/media/platform/msm/dvb/include/
ccflags-y += -Idrivers/misc/

obj-$(CONFIG_DVB_MPQ_DEMUX) += mpq-dmx-hw-plugin.o

mpq-dmx-hw-plugin-y := mpq_dmx_plugin_common.o

mpq-dmx-hw-plugin-y += mpq_sdmx.o

mpq-dmx-hw-plugin-$(CONFIG_DVB_MPQ_TSPP1) += mpq_dmx_plugin_tspp_v1.o

mpq-dmx-hw-plugin-$(CONFIG_DVB_MPQ_TSPP2) += mpq_dmx_plugin_tspp_v2.o

mpq-dmx-hw-plugin-$(CONFIG_DVB_MPQ_TSIF) += mpq_dmx_plugin_tsif.o


