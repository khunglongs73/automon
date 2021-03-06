/*

    This file is part of the Automon Project (OBD Diagnostics) - http://www.automon.io/
    Source Repository: https://github.com/donaloconnor/automon/
    
    Copyright (c) 2015, Donal O'Connor <donaloconnor@gmail.com>

    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    
*/

#include "automon.h"

using namespace AutomonKernel;

Rule::Rule()
{
    /* Create a script engine that will perform execution of our rule script */
    m_scriptEngine = new QScriptEngine();
}

bool Rule::addSensor(Sensor * sensor)
{
    /*
        This method adds a sensor to our rule. For every sensor listed in our rule, it has to be added
    */

    if (sensor == NULL)
    {
        /* Inputted sensor pointer is null so exit */
#ifdef DEBUGAUTOMON
        qDebug() << "Invalid sensor";
#endif
        return false;
    }

    /* Add the sensor pointer to our list of sensors */
    m_sensors.append(sensor);

    /* Create a regular string for our command name */
    char commandName[1024];

    /* Prepend our command, ie sensor 010D with an s, so we have s010D. This is required since variables
       in EmeaScript (javascript) cannot start with a number
    */

    QString copyIn = "s" + sensor->getCommand();

    /* Copy the QString into our regular string */
    strcpy(commandName, copyIn.toLatin1());

    /* Create a QScriptValue object */
    QScriptValue value(m_scriptEngine, 0.0);

    /* Create a variable in our script engine of the name our sensor with the s and a value of 0.0 */
    m_scriptEngine->globalObject().setProperty(commandName, value);

}

bool Rule::activate()
{
    /*
        This method is used to activate the rule so it starts listening to sensor's signals
        and if the rule is satisfied it emits a signal
    */


    bool found = false;

    /* The rule is empty so return! */
    if (m_rule.compare("") == 0)
        return false;

    /* For each sensor, ensure that no null pointers present */
    for (int i = 0; i < m_sensors.size(); i++)
        if (m_sensors[i] == NULL)
            return false;

    /* Get a list of the sensors in the rule string */
    QStringList sensorNames;
    sensorNames = m_rule.split(QRegExp("s"));

    for (int i = 0; i < sensorNames.size(); i++)
        if (sensorNames.at(i).compare("") != 0)
        {
            /* For each sensor that isn't NULL in the list, check if found in sensor list */
            found = false;

            for (int j = 0; j < m_sensors.size(); j++)
            {
                if (sensorNames.at(i).section("",0,4).compare(m_sensors.at(j)->getCommand()) == 0)
                    found = true; /* We have a match so safe */
            }

            if (!found)
                return false; /* Sensor wasn't found in list so can't active so return */
        }

    /* Ensure that the script engine can evaulate the inputted rule */
    if (!m_scriptEngine->canEvaluate(m_rule))
        return false;

    /* Connect each sensor's signal to this rule so we can get updates */
    for (int i = 0; i < m_sensors.size(); i++)
        connect(m_sensors[i], SIGNAL(changeOccurred(double)), this, SLOT(updateRule(double)));

    /* Activation successful */
    return true;
}

void Rule::updateRule(double value)
{
    /*
        This slot is called by each sensor in the rule when their values change.
        We update the value of the corresponding sensor in the script engine and do check
    */

    Sensor * senderSensor = static_cast<Sensor*>(QObject::sender());

    char commandName[1024];
    QString copyIn = "s" + senderSensor->getCommand();
    strcpy(commandName, copyIn.toLatin1());

    /* Set a new value to this */
    QScriptValue scriptValue(m_scriptEngine, value);

    /* Set the sXXXX sensor variable in the script engine to the value inputted */
    m_scriptEngine->globalObject().setProperty(commandName, scriptValue);

    /* Now do a check to see if the rule is satisfied */
    checkIfSatisfied();

}

bool Rule::validateRule()
{
    /* Ensures that the rule is not empty */
    if (m_rule.compare("") == 0)
        return false;

    return true;
}

void Rule::setRule(QString rule)
{
    /* A setter method to set the rule */
    m_rule = rule;
}

void Rule::setRuleName(QString ruleName)
{
    /* A setter to name the rule to an identifiable rule, like "Engine RPM < 30 AND Speed > 150 */
    m_ruleName = ruleName;
}

QString Rule::getRule()
{
    /* Return rule string */
    return m_rule;
}

QString Rule::getRuleName()
{
    /* Return rule human readable name */
    return m_ruleName;
}


bool Rule::checkIfSatisfied()
{
    /*
        This method is used to check if the current rule string is satisfied after sensor values were updated
    */

    /* Ensure that all sensors have updated first.
        Coolant Temperature might not get updated for a few seconds so it's initial value is 0.
        We will only validate rule when we have a proper up to date value of all sensors
    */

    for (int i = 0; i < m_sensors.size(); i++)
    {
        if (m_sensors.at(i)->getChangeTimes() < 1)
            return true;
    }

    /* Get the result of our rule expression */
    bool ruleResult = m_scriptEngine->evaluate(m_rule.toLatin1()).toBoolean();

    if (ruleResult != m_satisfied && ruleResult)
    {
        /* Rule satisfied, but only send update if not already */
        m_satisfied = ruleResult;
        sendAlert(m_ruleName);

    } else if (!ruleResult)
    {
        /* Rule not satisfied */
        m_satisfied = false;
    }

    return m_satisfied;
}
