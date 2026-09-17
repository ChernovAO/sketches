# Rapid — Java Development Guide

## Overview

Multi-module Maven project (Spring Boot 3.5.3, Java 25) for database synchronization, Kafka integration, and auxiliary services. Package root: `com.moex.tks.rapid`.

## Module Structure

| Module | Purpose |
|--------|---------|
| `dbsync` | Database synchronization service |
| `dbsync-generator` | Code generation for dbsync |
| `dbmodel` | Database model definitions |
| `dbmodel-generator` | Code generation for dbmodel |
| `kafka_db_store` | Kafka -> DB consumer/writer |
| `db_kafka_pub` | DB -> Kafka publisher |
| `interop` | Interoperability layer between C++ and Java |
| `proto/proto-rapid` | Protobuf definitions for Rapid protocol |
| `proto/proto-pingpong` | Protobuf definitions for ping-pong |
| `proto/proto-oi` | Protobuf definitions for OI |
| `utils` | Shared utilities |
| `zonar` | Monitoring/health service |
| `replay_app` / `replay_cmd` | Transaction replay tools |
| `settleccp_app` | Settlement CCP application |
| `xfers` | Transfer service |

## Build and Test

```bash
cd app_java

# Full build with tests
mvn clean install

# Single module
mvn -pl kafka_db_store clean install

# Run tests only
mvn test

# Single module tests
mvn -pl kafka_db_store test

# Single test class
mvn -Dtest=DataEventTest test

# Single test method
mvn -Dtest=DataEventTest#testMethod test
```

## Key Dependencies

- **Spring Boot 3.5.3** — application framework, dependency management
- **Lombok 1.18.42** — boilerplate reduction (`@Data`, `@Builder`, etc.)
- **Protobuf 3.19.4** — serialization
- **Mockito 5.20** — test mocking (requires `-javaagent` for Java 25)
- **Instancio 4.0** — random test data generation
- **JaCoCo 0.8.13** — code coverage

## Test Configuration

- Tests tagged `LongRunning` are excluded by default (`excluded.unittest.tags` property)
- Mockito requires the JVM argument: `-javaagent:.../mockito-core-5.20.0.jar` (configured in surefire plugin)
- Run with SonarQube analysis: `mvn -PSonar install sonar:sonar` (requires `SONAR_HOST_URL`, `SONAR_TOKEN`, `SONAR_PROJECTKEY_JAVA` env vars)

## Maven Repository

Nexus mirror must be configured in `~/.m2/settings.xml`:

```xml
<mirrors>
    <mirror>
        <id>moex-mirror-central</id>
        <name>moex-mirror-central</name>
        <url>https://nexus-dev.tech.moex.com/repository/moex-maven-static-group</url>
        <mirrorOf>central</mirrorOf>
    </mirror>
</mirrors>
```
