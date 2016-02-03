<?php
/**
 * This file is part of OpenMediaVault.
 *
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    Volker Theile <volker.theile@openmediavault.org>
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2009-2015 Volker Theile
 * @copyright Copyright (c) 2015 OpenMediaVault Plugin Developers
 *
 * OpenMediaVault is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * OpenMediaVault is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenMediaVault. If not, see <http://www.gnu.org/licenses/>.
 */

require_once "openmediavault/config.inc";
require_once "openmediavault/error.inc";
require_once "openmediavault/systemctl.inc";
require_once "openmediavault/module.inc";
require_once "openmediavault/util.inc";
require_once "openmediavault/monit.inc";
require_once "openmediavault/log.inc";

class OMVModuleMpd extends OMVModuleServiceAbstract implements
    OMVIModuleServiceStatus,
    OMVINotifyListener
{
    private $moduleName = "mpd";
    private $systemdServiceName = "mpd.service";

    private $configXPath = "/config/services/mpd";
    private $audioOutputsXPath = "/config/services/mpd/audiooutputs";
    private $individualAudioOutputXPath = "/config/services/mpd/audiooutputs/audiooutput";

    private $eventMessagePath = "org.openmediavault.services.mpd";
    private $audioOutputEventMessagePath = "org.openmediavault.services.mpd.audiooutputs.audiooutput";


    /**
     * Get the name of the module.
     *
     * @return string
     */
    public function getName()  { return $this->moduleName; }


    /**
     * Defines the modules that if present should start before the service
     * provided by this module.
     * @return An array of modules.
     */
    public function shouldStart()  { return array("monit"); }


    /**
     * Defines the modules that if present should be stopped after the
     * service provided by this module.
     * @return An array of modules.
     */
    function shouldStop()  { return array("monit"); }


    /**
     * Get an individual audio output configuration XPath by UUID
     *
     * @return string
     */
    private function getAudioOutputXPathByUuid($uuid)
    {
        return sprintf(
            "%s[uuid='%s']",
            $this->getAudioOutputXPath(),
            $uuid
        );
    }


    /**
     * Helper function to find out whether the given shared folder
     * configuration object is used. If it is used, then mark the
     * module as dirty.
     * @param type The event message type.
     * @param path The event message path.
     * @param object The configuration object.
     */
    final public function onModifySharedFolder($type, $path, $object) {
            global $xmlConfig;
            $xpath = $this->getAudioOutputXPathByUuid($object["uuid"]);
            if (TRUE === $xmlConfig->exists($xpath))
                    $this->setDirty();
    }

    /**
     * Get the status of the service.
     *
     * @return array
     */
    public function getStatus()
    {
        global $xmlConfig;

        $object = $xmlConfig->get($this->configXPath);

        if (is_null($object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->configXPath
            );
        }

        $monit = new OMVMonit($this->getName());
        return array(
            "name" => $this->getName(),
            "title" => gettext("MPD"),
            "enabled" => boolval($object["enable"]),
            "running" => $monit->isRunning()
        );
        /*
        $systemCtl = new OMVSystemCtl($this->systemdServiceName);
        return [
            "name" => $this->getName(),
            "title" => gettext("Skeleton"),
            "enabled" => boolval($object["enable"]),
            "running" => $systemCtl->isActive()
        ];
        */
    }

    /**
     * Create and update the configuration.
     *
     * @return  void
     */
    public function applyConfig()
    {
        global $xmlConfig;

        $cmd = sprintf("export LANG=C; omv-mkconf %s 2>&1", $this->getName());

        if (0 !== $this->exec($cmd, $output)) {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_FAILED,
                $cmd,
                implode("\n", $output)
            );
        }

        $object = $xmlConfig->get($this->configXPath);

        if (is_null($object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->configXPath
            );
        }

        if (!boolval($object["enable"])) {
            return;
        }

        $systemCtl = new OMVSystemCtl($this->systemdServiceName);
        $systemCtl->restart();
    }

    /**
     * Start the managed service.
     *
     * @return void
     */
    public function startService()
    {
        global $xmlConfig;

        $object = $xmlConfig->get($this->configXPath);

        if (is_null($object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->configXPath
            );
        }

        if (!$object["enable"]) {
            return;
        }

        // Start this service and enable the unit file.
       $systemCtl = new OMVSystemCtl($this->systemdServiceName);
       $systemCtl->enable(TRUE);
       // Wait to allow the service to start.
       sleep(1);

        // Enable monitoring of this service.
        $monit = new OMVMonit($this->getName());
        $monit->monitor();
    }

    /**
     * Stop the managed service.
     *
     * @return void
     */
    public function stopService()
    {
        // Disable monitoring of this service.
        $monit = new OMVMonit($this->getName());
        $monit->unmonitor(TRUE);
        // Stop this service and disable the unit file.
        $systemCtl = new OMVSystemCtl($this->systemdServiceName);
        $systemCtl->disable(TRUE);
    }

    /**
     * Bind listeners.
     *
     * @return void
     */
    public function bindListeners(OMVNotifyDispatcher $dispatcher)
    {
        $dispatcher->addListener(
            OMV_NOTIFY_MODIFY,
            $this->eventMessagePath,
            array($this, "setDirty")
        );

        $dispatcher->addListener(
            OMV_NOTIFY_CREATE | OMV_NOTIFY_MODIFY | OMV_NOTIFY_DELETE,
            $this->audioOutputEventMessagePath,
            array($this, "setDirty")
        );

        $dispatcher->addListener(
            OMV_NOTIFY_MODIFY,
            "org.openmediavault.system.shares.sharedfolder",
            array($this, "onModifySharedFolder")
        );

        $moduleMgr = &OMVModuleMgr::getInstance();
        $dispatcher->addListener(
                OMV_NOTIFY_MODIFY,
                $this->eventMessagePath,
                array($moduleMgr->getModule("monit"), "setDirty")
        );
    }
}

// Register module
$moduleMgr = &OMVModuleMgr::getInstance();
$moduleMgr->registerModule(new OMVModuleMpd());

// Register new logfile type.
// Format:
// Sep 01 13:43 : client: [62] command list size (2097163) is larger than the max (2097152)
// Sep 01 17:17 : player_thread: played "http://192.168.1.1:9790/minimserver/*/Original-FLAC/CDs/Albums/_Compilations/100*25*20R...
// Aug 30 12:45 : flac: FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC
OMVLogFile::registerType("mpd", array(
  "filename" => "mpd.log",
  "filepath" => "/var/log/mpd/mpd.log",
  "regex" => "/^(\S+\s+\d{2}\s+\d{2}:\d{2})\s:\s(\S+):\s+(.*)$/",
  "columns" => array(
      "date" => array(
          "index" => 1,
          "func" => create_function('$v', 'return strpdate($v,"M d H:i");')
      ),
      "domain" => 2,
      "message" => 3
  )));
