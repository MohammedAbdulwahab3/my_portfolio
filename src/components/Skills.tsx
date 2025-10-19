import { Card } from "@/components/ui/card";
import { Smartphone, Database, Server, Cloud } from "lucide-react";

const Skills = () => {
  const skillCategories = [
    {
      title: "Frontend",
      icon: <Smartphone className="w-8 h-8 text-accent" />,
      skills: ["Flutter", "Dart", "Responsive UI", "State Management"],
      gradient: "from-primary/20 to-accent/20"
    },
    {
      title: "Backend",
      icon: <Server className="w-8 h-8 text-primary" />,
      skills: ["Django", "Python", "Golang", "REST APIs"],
      gradient: "from-accent/20 to-primary/20"
    },
    {
      title: "Database",
      icon: <Database className="w-8 h-8 text-accent" />,
      skills: ["Firebase", "PostgreSQL", "NoSQL", "Real-time DB"],
      gradient: "from-primary/20 to-accent/20"
    },
    {
      title: "Cloud & DevOps",
      icon: <Cloud className="w-8 h-8 text-primary" />,
      skills: ["Firebase Cloud", "Docker", "CI/CD", "Cloud Functions"],
      gradient: "from-accent/20 to-primary/20"
    },
  ];

  return (
    <section className="py-20 px-4" id="skills">
      <div className="container mx-auto">
        <h2 className="text-4xl md:text-5xl font-bold text-center mb-4">
          Tech <span className="text-gradient">Stack</span>
        </h2>
        <p className="text-center text-muted-foreground mb-12 max-w-2xl mx-auto">
          Specialized in building fullstack applications with modern technologies
        </p>
        
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
          {skillCategories.map((category, index) => (
            <Card 
              key={index}
              className="glass p-6 hover:scale-105 transition-all duration-300 cursor-pointer group"
              style={{ 
                animationDelay: `${index * 100}ms`,
                animationFillMode: 'both'
              }}
            >
              <div className={`bg-gradient-to-br ${category.gradient} rounded-xl p-4 w-fit mb-4 group-hover:scale-110 transition-transform`}>
                {category.icon}
              </div>
              <h3 className="text-xl font-semibold mb-3">{category.title}</h3>
              <ul className="space-y-2">
                {category.skills.map((skill, idx) => (
                  <li key={idx} className="text-muted-foreground flex items-center">
                    <span className="w-2 h-2 bg-accent rounded-full mr-2" />
                    {skill}
                  </li>
                ))}
              </ul>
            </Card>
          ))}
        </div>
      </div>
    </section>
  );
};

export default Skills;
