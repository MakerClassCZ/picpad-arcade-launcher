# Contributing to PicoLibSDK

Welcome to the PicoLibSDK community! We're excited that you're interested in contributing to this alternative SDK for Raspberry Pico development. This guide will help you get started with contributing to the project.

## 🌟 How to Contribute

There are many ways to contribute to PicoLibSDK:

- **Documentation**: Improve guides, add examples, fix typos
- **Code Examples**: Create new example projects or improve existing ones
- **Bug Reports**: Help us identify and fix issues
- **Feature Requests**: Suggest new functionality or improvements
- **Hardware Support**: Add support for new devices or peripherals
- **Testing**: Test the SDK on different hardware configurations
- **Translation**: Help translate documentation to other languages

## 📋 Before You Start

### Prerequisites

- **Development Environment**: Set up PicoLibSDK development environment
- **Hardware**: Access to Raspberry Pico, Pico 2, or compatible boards
- **Toolchain**: ARM GCC toolchain installed and configured
- **Git**: Basic knowledge of Git and GitHub workflows

### Getting Familiar

1. **Read the Documentation**: Familiarize yourself with the [Quick Start Guide](public/src/content/docs/quick-start/5-minute-guide.md)
2. **Try Examples**: Run some [basic examples](public/src/content/docs/examples/basic/) to understand the SDK
3. **Explore the Codebase**: Browse the source code to understand the project structure
4. **Join Discussions**: Participate in GitHub Discussions to connect with the community

## 🚀 Getting Started

### 1. Fork and Clone

```bash
# Fork the repository on GitHub, then clone your fork
git clone https://github.com/YOUR-USERNAME/PicoLibSDK.git
cd PicoLibSDK

# Add the upstream repository
git remote add upstream https://github.com/ORIGINAL-OWNER/PicoLibSDK.git
```

### 2. Set Up Development Environment

```bash
# Install dependencies for documentation development
cd public
npm install

# Test the documentation site
npm run dev
```

### 3. Create a Branch

```bash
# Create a new branch for your contribution
git checkout -b feature/your-feature-name
# or
git checkout -b fix/your-bug-fix
# or  
git checkout -b docs/your-documentation-improvement
```

## 📝 Contribution Guidelines

### Code Contributions

#### Code Style
- **Indentation**: Use tabs (not spaces) for C/C++ code
- **Naming Convention**: 
  - Functions: `PascalCase` (e.g., `GPIO_DirOut`)
  - Variables: `snake_case` (e.g., `led_pin`)
  - Constants: `UPPER_CASE` (e.g., `MAX_BUFFER_SIZE`)
  - Types: `sPrefixedType` (e.g., `sUartConfig`)
- **Comments**: Use clear, concise comments explaining the "why" not just the "what"
- **File Headers**: Include appropriate copyright and license headers

```c
// Example of good code style
void GPIO_ConfigureLED(int pin) {
    // Configure pin as output for LED control
    GPIO_DirOut(pin);
    
    // Initialize to off state
    GPIO_Out0(pin);
    
    printf("LED configured on GPIO %d\n", pin);
}
```

#### Testing
- **Hardware Testing**: Test your code on actual hardware when possible
- **Multiple Devices**: Test on different Pico variants (RP2040, RP2350)
- **Edge Cases**: Test boundary conditions and error scenarios
- **Documentation**: Update relevant documentation and examples

#### Performance Considerations
- **Memory Usage**: Be mindful of RAM and flash usage
- **Timing Critical**: Mark time-critical code appropriately
- **Power Efficiency**: Consider power consumption in your implementations
- **Cross-Platform**: Ensure compatibility across supported devices

### Documentation Contributions

#### Writing Style
- **Clear and Concise**: Write in simple, clear language
- **Practical Examples**: Include working code examples
- **Step-by-Step**: Provide detailed instructions for complex procedures
- **Troubleshooting**: Include common issues and solutions

#### Documentation Structure
```markdown
---
title: Clear, Descriptive Title
description: Brief description for search and navigation
---

# Main Heading

Brief introduction explaining what this document covers.

## Section Heading

Content with practical examples:

```c
// Working code example
#include "include.h"

int main() {
    printf("Hello, PicoLibSDK!\n");
    return 0;
}
```

## Next Steps

Links to related documentation.
```

#### Content Guidelines
- **Accuracy**: Ensure all code examples work and are tested
- **Completeness**: Cover all necessary steps and prerequisites
- **Up-to-Date**: Keep information current with latest SDK versions
- **Inclusive**: Use inclusive language and consider different skill levels

### Example Projects

When contributing example projects:

#### Project Structure
```
ExampleName/
├── main.c              # Main source code
├── config.h            # Project configuration
├── Makefile            # Build configuration  
├── README.md           # Project documentation
├── build.sh            # Cross-platform build script
└── wiring-diagram.png  # Hardware connection diagram (if applicable)
```

#### README Template
```markdown
# Project Name

Brief description of what the project does.

## Hardware Requirements

- Raspberry Pico or Pico 2
- Additional components (LEDs, sensors, etc.)
- Breadboard and jumper wires

## Wiring

![Wiring Diagram](wiring-diagram.png)

| Component | Pico Pin | Description |
|-----------|----------|-------------|
| LED       | GPIO 25  | Status indicator |
| Button    | GPIO 2   | User input |

## Building

```bash
# Using picolib-build
picolib-build build --device pico

# Or using traditional build scripts
c.bat pico
```

## Usage

1. Flash the UF2 file to your Pico
2. Connect via serial terminal
3. Follow the on-screen instructions

## Learning Objectives

- GPIO input/output
- Serial communication
- [Other concepts covered]
```

## 🔄 Development Workflow

### 1. Development Process

```bash
# Keep your fork up to date
git fetch upstream
git checkout main
git merge upstream/main

# Create feature branch
git checkout -b feature/new-awesome-feature

# Make your changes
# ... edit files ...

# Test your changes
picolib-build build --device pico
# Test on hardware

# Commit your changes
git add .
git commit -m "Add new awesome feature

- Implement XYZ functionality
- Add comprehensive tests
- Update documentation
- Add example project"

# Push to your fork
git push origin feature/new-awesome-feature
```

### 2. Pull Request Process

1. **Create Pull Request**: Open a PR from your feature branch to the main repository
2. **Fill Template**: Use the provided PR template and fill in all sections
3. **Link Issues**: Reference any related issues using keywords (fixes #123)
4. **Review Process**: Respond to feedback and make requested changes
5. **Testing**: Ensure all CI checks pass
6. **Merge**: Once approved, your PR will be merged

### 3. Pull Request Template

```markdown
## Description

Brief description of changes made.

## Type of Change

- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update
- [ ] Performance improvement
- [ ] Code refactoring

## Hardware Tested

- [ ] Raspberry Pico (RP2040)
- [ ] Raspberry Pico 2 (RP2350 ARM)
- [ ] Raspberry Pico 2 (RP2350 RISC-V)
- [ ] PicoPad
- [ ] Other: ___________

## Testing Performed

- [ ] Code compiles without warnings
- [ ] Functionality tested on hardware
- [ ] Examples run successfully
- [ ] Documentation builds correctly
- [ ] No performance regressions

## Checklist

- [ ] My code follows the project's style guidelines
- [ ] I have performed a self-review of my code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I have made corresponding changes to the documentation
- [ ] My changes generate no new warnings
- [ ] I have added tests that prove my fix is effective or that my feature works
- [ ] New and existing unit tests pass locally with my changes

## Additional Notes

Any additional information, screenshots, or context.
```

## 🐛 Reporting Issues

### Bug Reports

Use the bug report template and include:

- **Environment**: OS, toolchain version, hardware
- **Expected Behavior**: What should happen
- **Actual Behavior**: What actually happens
- **Steps to Reproduce**: Minimal steps to reproduce the issue
- **Code Sample**: Minimal code that demonstrates the problem
- **Workarounds**: Any temporary solutions you've found

### Feature Requests

Use the feature request template and include:

- **Problem Statement**: What problem does this solve?
- **Proposed Solution**: How should it work?
- **Alternative Solutions**: Other approaches considered
- **Use Cases**: Real-world scenarios where this would be useful
- **Implementation Ideas**: Technical approach (if you have ideas)

## 📚 Documentation Guidelines

### Writing Documentation

1. **Audience**: Write for developers familiar with embedded systems
2. **Structure**: Use clear headings and logical organization
3. **Examples**: Include practical, working code examples
4. **Testing**: Verify all code examples work correctly
5. **Links**: Use relative links for internal documentation

### Documentation Development

```bash
# Start documentation development server
cd public
npm run dev

# Build documentation
npm run build

# Preview built site
npm run preview
```

### Adding New Pages

1. Create markdown files in `public/src/content/docs/`
2. Update navigation in `public/astro.config.mjs` if needed
3. Follow the established naming conventions
4. Include appropriate frontmatter

## 🏗️ Build System Contributions

### picolib-build Tool

The cross-platform build tool is written in Python. To contribute:

```bash
# Test the build tool
python3 tools/picolib-build --help

# Test project creation
python3 tools/picolib-build init test-project

# Test building
cd test-project
python3 ../tools/picolib-build build --device pico
```

### Adding New Device Support

To add support for a new device:

1. **Device Configuration**: Add device entry to `DeviceConfig.DEVICES` in `tools/picolib-build`
2. **Setup Script**: Update `_setup.bat` and `_setup.sh` with device parameters
3. **Makefile**: Add any device-specific build rules
4. **Documentation**: Create hardware guide for the new device
5. **Examples**: Ensure existing examples work on the new device

## 🎯 Specific Contribution Areas

### High-Priority Areas

1. **Hardware Drivers**: Support for new sensors, displays, communication modules
2. **Performance Optimization**: Improve speed and memory usage
3. **Cross-Platform Support**: Better Linux/macOS support
4. **Documentation**: More examples, tutorials, and guides
5. **Testing**: Automated testing and hardware-in-the-loop tests

### Beginner-Friendly Tasks

Look for issues labeled `good first issue` or `help wanted`:

- Fix typos in documentation
- Add missing code comments
- Create simple example projects
- Improve error messages
- Add hardware connection diagrams

### Advanced Contributions

- New peripheral drivers
- Performance optimizations
- Advanced examples (DSP, graphics, networking)
- Build system improvements
- Cross-compilation support

## 🤝 Community

### Communication Channels

- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General questions and community discussions
- **Pull Requests**: Code review and collaboration

### Code of Conduct

We are committed to providing a welcoming and inclusive environment for all contributors. Please:

- **Be Respectful**: Treat all community members with respect
- **Be Inclusive**: Welcome newcomers and different perspectives
- **Be Patient**: Help others learn and grow
- **Be Constructive**: Provide helpful feedback and suggestions
- **Follow Guidelines**: Adhere to project guidelines and standards

### Getting Help

If you need help:

1. **Search Existing Issues**: Your question might already be answered
2. **GitHub Discussions**: Ask general questions
3. **Documentation**: Check the comprehensive guides
4. **Examples**: Look at existing example projects

## 📄 Legal

### License

By contributing to PicoLibSDK, you agree that your contributions will be licensed under the same license as the project. Make sure you have the right to contribute any code you submit.

### Attribution

Significant contributions will be acknowledged in the project documentation and release notes. We appreciate all contributions, no matter how small!

---

## 🎉 Thank You!

Thank you for contributing to PicoLibSDK! Your contributions help make embedded development more accessible and enjoyable for everyone. Whether you're fixing a typo, adding a new feature, or helping other developers, every contribution matters.

**Happy coding!** 🚀