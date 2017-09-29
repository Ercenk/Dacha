sudo sed '/^if [ -z "$KAFKA_JVM_PERFORMANCE_OPTS" ]; then$/{
       $!{ N        
         s/^KAFKA_JVM_PERFORMANCE_OPTS="-server -XX:+UseG1GC -XX:MaxGCPauseMillis=20 -XX:InitiatingHeapOccupancyPercent=35 -XX:+DisableExplicitGC -Djava.awt.headless=true"$/KAFKA_JVM_PERFORMANCE_OPTS="-client -XX:+UseParNewGC -XX:+UseConcMarkSweepGC -XX:+CMSClassUnloadingEnabled -XX:+CMSScavengeBeforeRemark -XX:+DisableExplicitGC -Djava.awt.headless=true"\nfi/
         t sub-yes
         :sub-not
         P 
         D 
         :sub-yes 
       }    
     }' /usr/local/kafka/bin/kafka-run-class.sh 