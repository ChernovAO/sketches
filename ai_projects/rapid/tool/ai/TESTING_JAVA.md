# Rapid — Java Testing Guide

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

## Test Configuration

- Tests tagged `LongRunning` are excluded by default (`excluded.unittest.tags` property)
- Mockito requires the JVM argument: `-javaagent:.../mockito-core-5.20.0.jar` (configured in surefire plugin)
- Run with SonarQube analysis: `mvn -PSonar install sonar:sonar` (requires `SONAR_HOST_URL`, `SONAR_TOKEN`, `SONAR_PROJECTKEY_JAVA` env vars)

## Key Test Dependencies

- **Mockito 5.20** — test mocking (requires `-javaagent` for Java 25)
- **Instancio 4.0** — random test data generation
- **JaCoCo 0.8.13** — code coverage
