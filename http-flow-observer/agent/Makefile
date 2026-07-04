AGENT_ARTIFACT := target/http-flow-agent-1.0.0.jar

all: build

build:
	mvn -q -DskipTests package
	@echo "Agent built: $(AGENT_ARTIFACT)"

clean:
	mvn -q clean

.PHONY: all build clean
